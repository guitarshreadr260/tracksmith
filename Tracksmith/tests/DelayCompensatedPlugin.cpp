#include <juce_audio_processors/juce_audio_processors.h>

// Corrected JUCE 8 version of the user's latency example; test plugin only.
class DelayCompensatedPlugin final : public juce::AudioProcessor
{
public:
    DelayCompensatedPlugin()
        : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
            .withOutput("Output", juce::AudioChannelSet::stereo(), true)), delayBuffer(2, delaySamples)
    { addParameter(gain=new juce::AudioParameterFloat(juce::ParameterID("gain",1),"Test gain",juce::NormalisableRange<float>(0.f,1.f),1.f));setLatencySamples(delaySamples); }
    void prepareToPlay(double, int) override { delayBuffer.clear(); delayIndex = 0; setLatencySamples(delaySamples); }
    void releaseResources() override {}
    void reset() override { delayBuffer.clear(); delayIndex = 0; }
    bool isBusesLayoutSupported(const BusesLayout& buses) const override
    {
        const auto input = buses.getMainInputChannelSet();
        return input == buses.getMainOutputChannelSet() && (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo());
    }
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        for (int frame = 0; frame < buffer.getNumSamples(); ++frame)
        {
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                auto* samples = buffer.getWritePointer(channel); auto* delay = delayBuffer.getWritePointer(channel);
                const float old = delay[delayIndex]; delay[delayIndex] = samples[frame]*gain->get(); samples[frame] = old;
            }
            // Advance once per stereo frame, never once independently per channel.
            if (++delayIndex == delaySamples) delayIndex = 0;
        }
    }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    const juce::String getName() const override { return "Tracksmith Delay Test"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return getSampleRate() > 0 ? delaySamples / getSampleRate() : 0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& state) override { state.reset(); }
    void setStateInformation(const void*, int) override {}
private:
    juce::AudioParameterFloat* gain=nullptr;
    static constexpr int delaySamples = 512;
    int delayIndex = 0;
    juce::AudioBuffer<float> delayBuffer;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DelayCompensatedPlugin)
};
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DelayCompensatedPlugin(); }
