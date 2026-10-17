#include "StandaloneAudioSettings.h"
#include "Processor.h"
StandaloneAudioSettings::StandaloneAudioSettings(TONE3000Processor& p,std::function<void()> cb):processor(p),onDeviceStateChanged(std::move(cb)){}
StandaloneAudioSettings::~StandaloneAudioSettings()=default;
bool StandaloneAudioSettings::isAvailable(){return false;}
void StandaloneAudioSettings::changeListenerCallback(juce::ChangeBroadcaster*){}
void StandaloneAudioSettings::setInputMetering(bool){}
void StandaloneAudioSettings::InputLevelTap::clear(){}
void StandaloneAudioSettings::InputLevelTap::audioDeviceIOCallbackWithContext(const float* const*,int,float* const*,int,int,const juce::AudioIODeviceCallbackContext&){}
static juce::var hosted(){auto* o=new juce::DynamicObject;o->setProperty("ok",false);o->setProperty("error","Audio and MIDI devices are managed by Tracksmith.");return o;}
juce::var StandaloneAudioSettings::getState(){return hosted();}
juce::var StandaloneAudioSettings::getInputLevels(){return hosted();}
juce::var StandaloneAudioSettings::setDeviceType(const juce::String&){return hosted();}
juce::var StandaloneAudioSettings::setDevice(const juce::String&,const juce::String&){return hosted();}
juce::var StandaloneAudioSettings::setInputChannels(const juce::Array<juce::var>&){return hosted();}
juce::var StandaloneAudioSettings::setOutputPair(int){return hosted();}
juce::var StandaloneAudioSettings::setSampleRate(double){return hosted();}
juce::var StandaloneAudioSettings::setBufferSize(int){return hosted();}
juce::var StandaloneAudioSettings::setHearYourself(bool){return hosted();}
juce::var StandaloneAudioSettings::playTestTone(){return hosted();}
juce::var StandaloneAudioSettings::openControlPanel(){return hosted();}
juce::var StandaloneAudioSettings::restartDevice(){return hosted();}
juce::var StandaloneAudioSettings::openMicSettings(){return hosted();}
juce::var StandaloneAudioSettings::setMidiInputEnabled(const juce::String&,bool){return hosted();}
juce::var StandaloneAudioSettings::openBluetoothMidiPairing(){return hosted();}
