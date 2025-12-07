#pragma once

#include <pthread.h>
#include <mutex>
#include <stddef.h>

namespace rvc {

/**
 * Précision d'inférence pour la dégradation.
 */
enum class RVCPrecision {
    FP32,   // Précision maximale (Par défaut)
    FP16,   // Haute vitesse (Dégradé)
    INT8    // Vitesse ultime (Dégradé)
};

/**
 * Le gestionnaire de sécurité et de stabilité.
 * Gère les verrous de thread, la prévention du SWAP, et la dégradation en cas de surcharge.
 */
class LockManager {
public:
    // Pattern Singleton pour garantir une seule instance globale
    static LockManager* getInstance();
    
    // Supprimer les constructeurs de copie pour le singleton
    LockManager(LockManager const&) = delete;
    void operator=(LockManager const&) = delete;

    // --------------------------------------------------
    // Fonctions de Sécurité et de Temps Réel
    // --------------------------------------------------
    
    // Tente de définir la priorité SCHED_FIFO pour le thread appelant.
    bool setRealTimePriority();

    // Verrouille la mémoire dans la RAM physique pour éviter le SWAP (mlock).
    bool lockMemory(void* addr, size_t len);

    // --------------------------------------------------
    // Gestion de la Dégradation (Stabilité)
    // --------------------------------------------------

    // Déclenche le mode dégradé (FP16 et PLC) en cas de Jitter critique.
    void forceDegradation();
    
    // Tente de restaurer les performances normales.
    void restorePerformance();
    
    // Accesseurs d'état pour le Watchdog et l'IE Manager
    bool isDegradationModeActive() const;
    bool isPLCActive() const;
    RVCPrecision getCurrentPrecision() const;

private:
    LockManager();
    ~LockManager();

    // Singleton
    static LockManager* instance_;
    static std::mutex mutex_;
    
    // État du système
    std::mutex stateMutex_;
    bool isDegradationActive_;
    bool isPLCActive_;
    RVCPrecision currentRVCPrecision_;
};

} // namespace rvc
