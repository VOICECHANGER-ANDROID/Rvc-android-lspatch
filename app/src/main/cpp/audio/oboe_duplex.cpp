#include "audio/oboe_duplex.h"
#include <android/log.h>
#include <oboe/Oboe.h>
#include <unistd.h>
#include <sys/mman.h>
#include <algorithm>

// Définitions pour les Logs Android
#define LOG_TAG "RVC_OBOE_DUPLEX"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Le namespace 'oboe' est utilisé pour accéder aux classes Oboe
using namespace oboe;

namespace rvc {

// --- Implémentation de la Classe OboeDuplex ---

OboeDuplex* OboeDuplex::instance_ = nullptr;
std::mutex OboeDuplex::mutex_;

OboeDuplex* OboeDuplex::getInstance() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (instance_ == nullptr) {
        instance_ = new OboeDuplex();
    }
    return instance_;
}

OboeDuplex::OboeDuplex() {}

/**
 * Initialise les streams Oboe et démarre la boucle audio duplex.
 */
oboe::Result OboeDuplex::init(int sampleRate) {
    if (isInitialized_) {
        LOGI("Oboe Duplex déjà initialisé.");
        return Result::OK;
    }

    sampleRate_ = sampleRate;
    
    // 1. Construction du Stream d'Entrée (Microphone du Casque)
    AudioStreamBuilder inputBuilder;
    inputBuilder.setDirection(Direction::Input)
                .setPerformanceMode(PerformanceMode::LowLatency) // ULTRA-BASSE LATENCE
                .setFormat(AudioFormat::Float)                   // Format Float pour le DSP
                .setChannelCount(1)                              // Mono
                .setSampleRate(sampleRate_)
                .setCallback(this); // La classe elle-même gère le callback

    Result result = inputBuilder.openStream(inputStream_);
    if (result != Result::OK) {
        LOGE("Échec de l'ouverture du stream d'entrée Oboe: %s", convertTo<ctrl63>
