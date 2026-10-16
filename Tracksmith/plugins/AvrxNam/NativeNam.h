#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

// Keep Eigen/NAM headers out of the editor and public guitar-engine headers.
class NativeNam
{
public:
    NativeNam();
    ~NativeNam();
    bool load (const juce::File&, double rate, int block, juce::String& error, bool allowRatePending = false);
    bool prepare (double rate, int block, juce::String& error);
    void process (juce::AudioBuffer<float>&, int frames);
    bool ready() const;
    juce::File file() const;
    void clear();
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
