#pragma once
#include <string>
#include <string_view>

class Processor {
public:
    virtual ~Processor() = default;
    
    // Prepares the processor with sample rate and max block size.
    virtual void prepare(double sampleRate, int maxBlockSize) = 0;
    
    // Processes audio blocks.
    // inputChannels: array of input channel pointers (read-only buffers).
    // numInputChannels: number of input channels.
    // outputChannels: array of output channel pointers (write-only/read-write buffers).
    // numOutputChannels: number of output channels.
    // numSamples: block size.
    virtual void process(const float* const* inputChannels, int numInputChannels,
                        float* const* outputChannels, int numOutputChannels,
                        int numSamples) = 0;
                        
    // Resets any internal state (delays, biquad histories, etc.).
    virtual void reset() = 0;
    
    // Parameter control
    virtual void setParameter(int index, float value) = 0;
    virtual float getParameter(int index) const = 0;
    virtual std::string getParameterName(int index) const = 0;
    virtual int getNumParameters() const = 0;
    
    // Custom string settings (e.g. file paths, asset hashes)
    virtual void setSetting(std::string_view key, std::string_view value) {}
    virtual std::string getSetting(std::string_view key) const { return ""; }
    
    // Latency reporting in samples (needed for NAM/convolution etc. if any)
    virtual int getLatencySamples() const { return 0; }
    
    // Bypass control
    virtual bool isBypassed() const = 0;
    virtual void setBypass(bool bypassed) = 0;
    
    // Unique ID of the processor type for presets
    virtual std::string getIdentifier() const = 0;
    
    // Friendly name of the processor
    virtual std::string getName() const = 0;
};
