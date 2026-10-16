#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include "InputRouting.h"

// Local labels are tied to the driver/device combination, not a session's media.
struct IOSettings
{
    juce::String key;
    std::array<juce::String, InputRouting::maximumInputs> inputs;
    std::array<juce::String, InputRouting::maximumOutputs> outputs;
    static juce::String deviceKey(juce::AudioDeviceManager& devices)
    {
        if (!devices.getCurrentAudioDevice()) return {};
        const auto setup = devices.getAudioDeviceSetup();
        return devices.getCurrentAudioDeviceType() + " | " + setup.inputDeviceName + " | " + setup.outputDeviceName;
    }
    juce::File file() const
    {
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("Tracksmith/IO").getChildFile(juce::String::toHexString(key.hashCode64()) + ".json");
    }
    void refresh(juce::AudioDeviceManager& devices)
    {
        const auto next = deviceKey(devices); if (next == key) return;
        key = next; inputs.fill({}); outputs.fill({}); if (key.isEmpty()) return;
        const auto source = file(); if (!source.existsAsFile() || source.getSize() > 65536) return;
        const auto value = juce::JSON::parse(source.loadFileAsString());
        if (value.getProperty("device", {}).toString() != key) return;
        const auto load = [&](const char* name, auto& labels) {
            const auto list = value.getProperty(name, {});
            if (const auto* array = list.getArray())
                for (int i = 0; i < std::min(array->size(), static_cast<int>(labels.size())); ++i)
                    if ((*array)[i].isString()) labels[i] = (*array)[i].toString().trim().substring(0, 48);
        };
        load("inputs", inputs); load("outputs", outputs);
    }
    juce::Result save() const
    {
        if (key.isEmpty()) return juce::Result::fail("Connect an audio device before saving I/O labels.");
        auto* object = new juce::DynamicObject(); juce::var value(object); object->setProperty("device", key);
        juce::Array<juce::var> in, out; for (const auto& label : inputs) in.add(label); for (const auto& label : outputs) out.add(label);
        object->setProperty("inputs", in); object->setProperty("outputs", out);
        const auto destination = file(); const auto made = destination.getParentDirectory().createDirectory(); if (made.failed()) return made;
        juce::TemporaryFile temporary(destination);
        return temporary.getFile().replaceWithText(juce::JSON::toString(value)) && temporary.overwriteTargetFileWithTemporary()
            ? juce::Result::ok() : juce::Result::fail("I/O labels could not be saved.");
    }
};
