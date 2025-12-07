#include "security/lock_manager.h"
#include <android/log.h>
#include <pthread.h>
#include <sched.h>
#include <sys/mman.h>
#include <string>

#define LOG_TAG "RVC_LOCK_MANAGER"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// --- Définitions Statiques ---

namespace rvc {

// Instance Singleton
LockManager* LockManager::instance_ = nullptr;
std::mutex LockManager::mutex_;

LockManager* LockManager::getInstance() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (instance_ == nullptr) {
        instance_ = new LockManager();
    }
    return instance_;
}

LockManager::LockManager() 
    : isDegradationActive_(false), 
      isPLCActive_(false),
      currentRVCPrecision_(RVCPrecision::FP32) {
    LOGI("LockManager initialisé.");
}

LockManager::~LockManager() {
    // Si la mémoire était verrouillée par mlock, la libérer ici.
}

// ----------------------------------------------------------------------
// I. Gestion de la Priorité du Thread (Verrouillage Temps Réel V1.0)
// ----------------------------------------------------------------------

/**
 * Applique la politique de planification SCHED_FIFO et la haute priorité.
 * (V1.0 : Priorité Absolue)
 */
bool LockManager::setRealTimePriority() {
    struct sched_param param;
    
    // Obtenir la priorité maximale pour SCHED_FIFO (généralement 99)
    int max_priority = sched_get_priority_max(SCHED_FIFO);
    if (max_priority == -1) {
        LOGE("Impossible d'obtenir la priorité maximale SCHED_FIFO.");
        return false;
    }

    param.sched_priority = max_priority;

    // Tente d'appliquer la politique SCHED_FIFO au thread actuel.
    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &param) != 0) {
        LOGE("Échec de l'application de SCHED_FIFO (Priorité: %d). Code d'erreur: %s", max_priority, strerror(errno));
        // L'échec est souvent dû aux restrictions Android. On doit s'assurer que c'est un thread créé par A/AAudio ou Oboe.
        return false;
    }

    LOGI("Priorité du thread temps réel verrouillée sur SCHED_FIFO (Prio: %d).", max_priority);
    return true;
}

/**
 * Tente de verrouiller la zone de mémoire du modèle RVC/Buffer dans la RAM physique.
 * (V13.0 : Verrouillage de Fichiers Modèles et SWAP)
 */
bool LockManager::lockMemory(void* addr, size_t len) {
    if (mlock(addr, len) == 0) {
        LOGI("Mémoire verrouillée (mlock) à l'adresse %p, taille %zu.", addr, len);
        return true;
    } else {
        LOGE("Échec du verrouillage mlock (SWAP prevention): %s", strerror(errno));
        // Avertissement, mais le système peut continuer.
        return false;
    }
}

// ----------------------------------------------------------------------
// II. Algorithme de Dégradation Gratuite et Résilience (V9.0/V12.0)
// ----------------------------------------------------------------------

/**
 * Déclenche le mode de dégradation (appelé par le Watchdog).
 * (V9.0 : Algorithme de Dégradation Gratuite)
 */
void LockManager::forceDegradation() {
    std::lock_guard<std::mutex> lock(stateMutex_);
    if (isDegradationActive_) return; // Déjà en mode dégradé

    isDegradationActive_ = true;
    currentRVCPrecision_ = RVCPrecision::FP16; // Bascule la précision
    isPLCActive_ = true; // Active l'interpolation pour compenser

    LOGE("🚨 ALERTE STABILITÉ : Mode de Dégradation Gratuite activé (FP16/PLC). Latence garantie.");
    
    // Après 5 secondes (ou un certain nombre de paquets), on tente de restaurer la stabilité
    // Nous aurions besoin d'un thread ou d'un compteur pour gérer la restauration.
}

/**
 * Restaure l'état de performance normale (appelé par le Watchdog après stabilisation).
 */
void LockManager::restorePerformance() {
    std::lock_guard<std::mutex> lock(stateMutex_);
    if (!isDegradationActive_) return;

    isDegradationActive_ = false;
    currentRVCPrecision_ = RVCPrecision::FP32; // Retour à la précision maximale
    isPLCActive_ = false; 

    LOGI("🟢 Stabilité Restaurée. Retour au mode FP32/Haute Qualité.");
}

// ----------------------------------------------------------------------
// III. Accesseurs d'État
// ----------------------------------------------------------------------

bool LockManager::isDegradationModeActive() const {
    return isDegradationActive_;
}

bool LockManager::isPLCActive() const {
    return isPLCActive_;
}

RVCPrecision LockManager::getCurrentPrecision() const {
    return currentRVCPrecision_;
}

} // namespace rvc
