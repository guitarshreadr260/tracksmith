#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_cryptography/juce_cryptography.h>
#include <thread>
namespace TracksmithGrooveFull {

// Select-flow client. HTTP, callback waiting and disk IO run off the audio/UI
// threads. The application consumes a downloaded file on the message thread.
class Tone3000
{
public:
    struct Model { juce::String id, name, url; };
    struct Reply { int status = 0; juce::MemoryBlock body; juce::StringPairArray headers; };
    using Transport = std::function<Reply (const juce::String&, const juce::String&, const juce::String&)>;
    Tone3000() = default;
    ~Tone3000();
    void configure (const juce::File& stateFile, const juce::File& downloadFolder);
    bool setKey (juce::String);
    juce::String getKey() const;
    juce::String status() const;
    juce::String title() const;
    juce::String creator() const;
    juce::String toneUrl() const;
    std::vector<Model> models() const;
    std::atomic<bool> busy { false };
    std::atomic<int> modelRevision { 0 };
    bool browse();
    bool download (int modelIndex);
    void cancel() { canceled.store (true); }
    void disconnect();
    void wait();
    juce::File takeReady();
    void reportStatus (juce::String s) { message (std::move (s)); }
    // Pure protocol helpers also used by the offline integration checks.
    static bool validKey (const juce::String&);
    static juce::String challenge (const juce::String& verifier);
    static juce::URL selectUrl (const juce::String& key, const juce::String& redirect, const juce::String& state, const juce::String& verifier);
    static juce::StringPairArray callback (const juce::String& requestTarget, const juce::String& expectedState);
    static std::vector<Model> parseModels (const juce::var&);
    static bool protect (const juce::MemoryBlock&, juce::MemoryBlock&);
    static bool unprotect (const juce::MemoryBlock&, juce::MemoryBlock&);
    // Dependency injection keeps tests offline; production always uses HTTPS.
    void setTestTransport (Transport value) { transport = std::move (value); }
    void setTestCallback (std::function<juce::String (const juce::URL&)> value) { testCallback = std::move (value); }
    std::function<bool (const juce::URL&)> openBrowser;
private:
    bool start (std::function<void()>);
    void select();
    Reply request (const juce::String& url, const juce::String& body, const juce::String& headers, size_t maxBytes = 2 * 1024 * 1024);
    juce::var jsonRequest (const juce::String& path, const juce::String& body = {}, bool authenticated = false);
    void tokens (const juce::var&);
    void refresh();
    void store();
    void message (juce::String);
    static juce::String randomCode();
    static juce::String form (std::initializer_list<std::pair<juce::String, juce::String>>);
    mutable juce::CriticalSection lock;
    juce::String key, access, refreshToken, selectedTitle, selectedCreator, selectedUrl, selectedTone;
    juce::String messageText { "Set up your TONE3000 connection to browse NAM captures." };
    double expiresAt = 0;
    std::vector<Model> available;
    juce::File statePath, cacheFolder, readyFile;
    std::atomic<bool> canceled { false };
    std::thread worker;
    Transport transport;
    std::function<juce::String (const juce::URL&)> testCallback;
};
}
