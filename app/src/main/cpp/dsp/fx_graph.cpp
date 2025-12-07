#include "dsp/fx_graph.h"
#include "security/lock_manager.h" // Pour les checks de dégradation
#include <cmath>
#include <algorithm>
#include <android/log.h>

#define LOG_TAG "RVC_FX_GRAPH"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// --- Définitions des Stubs d'Effets Simples ---

namespace rvc {

/**
 * Simule un Noise Gate et l'Annulation d'Écho Acoustique (AEC).
 */
void AcousticEchoCanceller::process(float* buffer, size_t numSamples) {
    // V15.0: Logique d'AEC ici. Soustrait le signal de sortie du casque du signal d'entrée.
    // L'implémentation réelle nécessiterait un buffer de référence du signal de sortie.
    // Pour l'instant, on applique un Noise Gate léger.
    const float threshold = 0.005f; // Exemple de seuil
    for (size_t i = 0; i < numSamples; ++i) {
        if (std::abs(buffer[i]) < threshold) {
            buffer[i] = 0.0f; // Silence si sous le seuil
        }
    }
}

/**
 * Simule la Suppression de Bruit Neuronale (DNS) ou un filtre spectral avancé.
 */
void NoiseSuppressor::process(float* buffer, size_t numSamples) {
    // V9.0: Modèle TFLite ultra-léger pour la DNS s'exécutant ici.
    // Ceci serait implémenté comme un petit moteur d'inférence distinct.
    // Pour l'instant, simule un filtre passe-bas très simple pour le lissage.
    for (size_t i = 1; i < numSamples; ++i) {
        buffer[i] = buffer[i] * 0.95f + buffer[i-1] * 0.05f;
    }
}

/**
 * Simule le Compresseur Multibandes et le Limiteur de Crête.
 */
void MultibandCompressor::process(float* buffer, size_t numSamples) {
    // V1.0: Applique un Limiteur de Crête pour éviter le clipping dans le casque.
    const float limit = 0.99f;
    for (size_t i = 0; i < numSamples; ++i) {
        buffer[i] = std::max(-limit, std::min(limit, buffer[i]));
    }
}

/**
 * Algorithme de Compensation de Perte de Paquets (PLC - Packet Loss Concealment).
 */
void PacketLossConcealer::process(float* buffer, size_t numSamples) {
    // V12.0: Active l'interpolation temporelle si une perte de paquet est détectée.
    if (isActive) {
        // Logique : Synthétiser un signal court basé sur la dernière fréquence F0 connue.
        // Ceci est une implémentation complexe non détaillée ici, mais cruciale.
        // Pour simuler: on envoie un fade-out rapide du dernier son.
        for (size_t i = 0; i < numSamples; ++i) {
            buffer[i] = buffer[i] * (1.0f - (float)i / numSamples); // Fade out
        }
    }
}

// --- Implémentation de la Classe FXGraph (Le Graphe Modulaire) ---

FXGraph::FXGraph(int sampleRate) : sampleRate_(sampleRate) {
    // Initialisation de la chaîne de traitement (Ordre critique)
    LOGI("Initialisation du Graphe de Traitement Audio à %d Hz.", sampleRate);

    // V7.0: Architecture de Plugins/Nœuds Modulaires
    // Initialisation des effets
    aec_ = std::make_unique<AcousticEchoCanceller>();
    ns_ = std::make_unique<NoiseSuppressor>();
    plc_ = std::make_unique<PacketLossConcealer>();
    compressor_ = std::make_unique<MultibandCompressor>();
    
    isInitialized_ = true;
}

FXGraph::~FXGraph() {
    LOGI("Destruction du Graphe de Traitement Audio.");
    // unique_ptr s'occupe de la désallocation des effets.
}

/**
 * Étape 1: Pré-Traitement du Signal (Avant RVC).
 */
void FXGraph::applyAcousticPreprocessing(float* buffer, size_t numSamples) {
    if (!isInitialized_) return;

    // 1. Annulation d'Écho Acoustique (AEC - Stabilité Casque)
    aec_->process(buffer, numSamples); 

    // 2. Suppression de Bruit Neuronale (DNS - Qualité d'entrée)
    ns_->process(buffer, numSamples);
    
    // Autres : Pré-Égalisation, correction de phase de l'entrée.
}

/**
 * Étape 2: Post-Traitement du Signal (Après RVC).
 */
void FXGraph::applyPostProcessing(float* buffer, size_t numSamples) {
    if (!isInitialized_) return;

    // V12.0: Vérification du mode dégradé (PLC)
    if (LockManager::getInstance()->isPLCActive()) {
        plc_->activate(); // Active le PLC si l'erreur est détectée par le Watchdog
        plc_->process(buffer, numSamples);
    } else {
        plc_->deactivate();
        
        // 1. Compresseur Multibandes et Limiteur (Qualité de sortie stable)
        compressor_->process(buffer, numSamples);

        // 2. Correction de la distorsion harmonique (V9.0)
        // Correction de la distorsion après le RVC...
        
        // 3. Réverbération / EQ final (effets utilisateur)
    }
}

/**
 * Mode léger, utilisé quand le RVC est désactivé (Low Power Pass-Through).
 */
void FXGraph::applyLowPowerDSP(float* buffer, size_t numSamples) {
    if (!isInitialized_) return;

    // Applique seulement le Noise Gate (AEC) et le Limiteur de Crête pour la communication simple.
    aec_->process(buffer, numSamples); 
    compressor_->process(buffer, numSamples);
}

} // namespace rvc
