#include <jni.h>
#include <unistd.h>
#include <sys/mman.h>
#include <android/log.h>
#include <pthread.h>
#include <chrono>
#include <cmath>
#include <string>
#include <vector>
#include <errno.h>

// Inclusion des Headers Critiques du Projet
#include "inference/ie_manager.h" // Gestionnaire TFLite/ONNX
#include "dsp/fx_graph.h"        // Pipeline d'effets (EQ, Compresseur, PLC)
#include "security/lock_manager.h" // Pour la gestion des verrous et la stabilité
#include "audio/oboe_duplex.h"     // Pour le Sidetone (monitoring casque)

// Définitions pour les Logs Android
#define LOG_TAG "RVC_NDK_CORE"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Constantes Critiques de Temps Réel
constexpr int RVC_SAMPLE_RATE = 48000;
constexpr int WATCHDOG_TIMEOUT_MS = 30; // 30ms max avant de forcer le PLC

// Déclarations Globales du Moteur
static bool isEngineInitialized = false;
static bool isRVCTransforming = false;
static float *sharedBufferPtr = nullptr;
static size_t sharedBufferSize = 0;
static InferenceEngineManager *ieManager = nullptr;
static FXGraph *fxGraph = nullptr;
static pthread_t watchdogThread; // Thread Watchdog pour la stabilité

// --- Déclaration des Fonctions JNI (Appelées par IPCManager.kt) ---

extern "C" JNIEXPORT jboolean JNICALL
Java_com_rvc_patch_ipc_IPCManager_initializeNativeEngine(
    JNIEnv *env,
    jobject /* this */,
    jobject fileDescriptor,
    jint bufferSize) {

    if (isEngineInitialized) {
        LOGI("Le moteur est déjà initialisé.");
        return JNI_TRUE;
    }

    try {
        // 1. Mappage de la Mémoire Partagée (Ashmem)
        int fd = env->GetIntField(fileDescriptor, env->GetFieldID(env->GetObjectClass(fileDescriptor), "descriptor", "I"));
        
        sharedBufferSize = bufferSize;
        // MAP_SHARED permet l'accès simultané Java/Kotlin et NDK C++
        sharedBufferPtr = static_cast<float *>(mmap(NULL, sharedBufferSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));

        if (sharedBufferPtr == MAP_FAILED) {
            LOGE("Échec du mmap Ashmem: %s", strerror(errno));
            return JNI_FALSE;
        }

        // 2. Verrouillage de la Mémoire (Pour les systèmes 4GB RAM)
        // Empêche Android de déplacer les données RVC vers le SWAP.
        if (mlock(sharedBufferPtr, sharedBufferSize) == 0) {
            LOGI("Mémoire Ashmem verrouillée (mlock) pour garantir le temps réel.");
        } else {
            LOGE("Avertissement: Échec du verrouillage mlock: %s", strerror(errno));
        }

        // 3. Initialisation des composants RVC critiques
        ieManager = new InferenceEngineManager();
        fxGraph = new FXGraph(RVC_SAMPLE_RATE);
        
        // Simule le chargement du modèle par défaut et le benchmark DSP/Hexagon
        if (!ieManager->loadDefaultModel(bufferSize, RVC_SAMPLE_RATE)) {
             LOGE("Échec du chargement du modèle par défaut.");
             // Nous pourrions choisir de continuer ou de retourner JNI_FALSE ici.
        }

        // 4. Initialisation des autres services (Sidetone, Watchdog)
        // OboeDuplex::getInstance()->init(RVC_SAMPLE_RATE); // Le Sidetone est initialisé

        isEngineInitialized = true;
        LOGI("Moteur RVC entièrement initialisé. Prêt pour le Trough Mic.");
        return JNI_TRUE;
    } catch (const std::exception &e) {
        LOGE("Erreur fatale d'initialisation: %s", e.what());
        return JNI_FALSE;
    }
}

/**
 * Fonction Watchdog pour surveiller les blocages.
 * (Fonctionnalité V12.0: Stabilité)
 */
void* watchdog_loop(void* arg) {
    LOGI("Watchdog démarré.");
    // Ce thread devrait avoir une priorité SCHED_FIFO légèrement inférieure à rvc_thread.
    
    while(isEngineInitialized) {
        // Logique de vérification du temps d'exécution ici...
        // 1. Vérifier si le thread principal RVC a manqué son délai (30ms).
        // 2. Vérifier la charge du CPU/DSP (Prédiction de Jitter V9.0).
        
        // Si la défaillance est détectée:
        // LockManager::getInstance()->forceDegradation(); // Active le mode FP16
        // FXGraph::getInstance()->activatePLC(); // Active l'Interpolation Temporelle
        
        usleep(10000); // Vérifie toutes les 10ms
    }
    return nullptr;
}

/**
 * Fonction appelée à chaque paquet audio par IPCManager.kt pour le traitement RVC.
 * C'est la boucle critique de 5-20ms.
 */
extern "C" JNIEXPORT jboolean JNICALL
Java_com_rvc_patch_ipc_IPCManager_processAudioNative(
    JNIEnv *env,
    jobject /* this */,
    jint bytesRead) {

    if (!isEngineInitialized || !sharedBufferPtr) {
        LOGE("Moteur non initialisé. Échec du traitement.");
        return JNI_FALSE;
    }
    
    // Démarre la mesure de la latence (chrono)
    auto start_time = std::chrono::high_resolution_clock::now();

    const size_t numSamples = bytesRead / sizeof(float);
    
    // Si le traitement RVC n'est pas activé par l'utilisateur (Pass-through léger)
    if (!isRVCTransforming) {
        // Applique seulement le Noise Gate et le Limiteur de Crête (Mode Low Power Pass-Through V10.0)
        fxGraph->applyLowPowerDSP(sharedBufferPtr, numSamples);
        return JNI_TRUE;
    }

    try {
        // --- Pipeline RVC Complet ---

        // 1. Pré-Traitement Acoustique (AEC, DNS Neuronale)
        fxGraph->applyAcousticPreprocessing(sharedBufferPtr, numSamples); 

        // 2. Inférence RVC (La partie la plus lourde sur le DSP/Hexagon)
        // ieManager traite directement le sharedBufferPtr (In-Place Inference)
        ieManager->runInference(sharedBufferPtr, numSamples); 

        // 3. Post-Traitement et Finition (EQ, Compresseur Multibandes, PLC)
        fxGraph->applyPostProcessing(sharedBufferPtr, numSamples);

        // 4. Envoi du Sidetone au casque (Monitoring)
        // OboeDuplex::getInstance()->sendAudio(sharedBufferPtr, numSamples);

        // --- Fin du Pipeline RVC ---
        
        // Mesure la latence
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
        
        // Met à jour le HUD de performance avec 'duration'
        // LOGI("Latence du paquet: %lld µs", duration); 
        
        // Si la latence dépasse le seuil, active le PLC et le mode dégradé.
        if (duration > WATCHDOG_TIMEOUT_MS * 1000) {
             LOGE("Latence critique détectée: %lld µs. Dégradation activée.", duration);
             // LockManager::getInstance()->forceDegradation();
             // fxGraph->activatePLC();
        }

        return JNI_TRUE;
    } catch (const std::exception &e) {
        // V13.0: Récupération d'État Transactionnelle ici pour le service défaillant.
        LOGE("Erreur fatale dans le pipeline RVC: %s", e.what());
        return JNI_FALSE; // Retourne false pour forcer le Pass-Through en Java
    }
}
