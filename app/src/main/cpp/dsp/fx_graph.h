#pragma once

#include <memory>
#include <stddef.h>
#include <string>

namespace rvc {

// Interface de base pour tous les processeurs d'effets
class AudioProcessor {
public:
    virtual ~AudioProcessor() = default;
    virtual void process(float* buffer, size_t numSamples) = 0;
};

// Déclarations des effets (Stubs)
class AcousticEchoCanceller : public AudioProcessor {
public:
    void process(float* buffer, size_t numSamples) override;
};

class NoiseSuppressor : public AudioProcessor {
public:
    void process(float* buffer, size_t numSamples) override;
};

class MultibandCompressor : public AudioProcessor {
public:
    void process(float* buffer, size_t numSamples) override;
};

class PacketLossConcealer : public AudioProcessor {
public:
    void process(float* buffer, size_t numSamples) override;
    void activate() { isActive = true; }
    void deactivate() { isActive = false; }
private:
    bool isActive = false;
};

/**
 * La classe principale qui orchestre le pipeline d'effets (le Graphe Modulaire).
 */
class FXGraph {
public:
    FXGraph(int sampleRate);
    ~FXGraph();

    // Appliqué avant le moteur RVC
    void applyAcousticPreprocessing(float* buffer, size_t numSamples);
    
    // Appliqué après le moteur RVC
    void applyPostProcessing(float* buffer, size_t numSamples);

    // Utilisé en mode Pass-Through
    void applyLowPowerDSP(float* buffer, size_t numSamples);

private:
    bool isInitialized_ = false;
    int sampleRate_;
    
    // Modules d'effets
    std::unique_ptr<AcousticEchoCanceller> aec_;
    std::unique_ptr<NoiseSuppressor> ns_;
    std::unique_ptr<PacketLossConcealer> plc_;
    std::unique_ptr<MultibandCompressor> compressor_;
};

} // namespace rvc
