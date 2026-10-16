#include "Tone3000.h"
#if JUCE_WINDOWS
#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>
#endif
namespace TracksmithGrooveFull {

namespace
{
const juce::String api = "https://www.tone3000.com/api/v1/";
juce::String text (const juce::var& v, const char* name) { return v.getProperty (name, {}).toString(); }
bool toneHost (const juce::URL& u) { return u.getScheme() == "https" && (u.getDomain() == "www.tone3000.com" || u.getDomain() == "tone3000.com"); }
juce::var parse (const Tone3000::Reply& r) { const auto v = juce::JSON::parse (juce::String::fromUTF8 (static_cast<const char*> (r.body.getData()), static_cast<int> (r.body.getSize()))); if (! v.isObject()) throw std::runtime_error ("TONE3000 returned an unreadable response. Try again."); return v; }
void requireReply (const Tone3000::Reply& r) { if (r.status == 429) throw std::runtime_error ("TONE3000 is busy. Wait a minute before trying again."); if (r.status < 200 || r.status >= 300) throw std::runtime_error (("TONE3000 request failed (" + juce::String (r.status) + "). Check connection or reconnect.").toStdString()); }
}
Tone3000::~Tone3000() { cancel(); wait(); }
void Tone3000::wait() { if (worker.joinable()) worker.join(); }
juce::String Tone3000::getKey() const { const juce::ScopedLock guard (lock); return key; }
juce::String Tone3000::status() const { const juce::ScopedLock guard (lock); return messageText; }
juce::String Tone3000::title() const { const juce::ScopedLock guard (lock); return selectedTitle; }
juce::String Tone3000::creator() const { const juce::ScopedLock guard (lock); return selectedCreator; }
juce::String Tone3000::toneUrl() const { const juce::ScopedLock guard (lock); return selectedUrl; }
std::vector<Tone3000::Model> Tone3000::models() const { const juce::ScopedLock guard (lock); return available; }
void Tone3000::message (juce::String s) { const juce::ScopedLock guard (lock); messageText = std::move (s); }
bool Tone3000::validKey (const juce::String& s) { return s.startsWith ("t3k_pub_") && s.length() > 12 && s.length() < 256 && s.containsOnly ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-"); }
bool Tone3000::setKey (juce::String s)
{
    s = s.trim(); if (busy.load()) return false;
    if (! validKey (s)) { message ("Enter a publishable t3k_pub_ key from TONE3000 Settings / API Keys. Secret keys are not accepted."); return false; }
    { const juce::ScopedLock guard (lock); if (key != s) { key = s; access.clear(); refreshToken.clear(); expiresAt = 0; available.clear(); modelRevision.fetch_add (1); } }
    store(); message ("Ready. Browse TONE3000 to sign in and choose an Amp + Cab NAM capture."); return true;
}
void Tone3000::configure (const juce::File& stateFile, const juce::File& folder)
{
    if (busy.load()) return; statePath = stateFile; cacheFolder = folder;
    juce::MemoryBlock encrypted, clear;
    if (stateFile.getSize() <= 65536 && stateFile.loadFileAsData (encrypted) && unprotect (encrypted, clear))
    {
        const auto v = juce::JSON::parse (juce::String::fromUTF8 (static_cast<const char*> (clear.getData()), static_cast<int> (clear.getSize())));
        const auto candidate = text (v, "client_id"); if (validKey (candidate)) { const juce::ScopedLock guard (lock); key = candidate; access = text (v, "access_token"); refreshToken = text (v, "refresh_token"); expiresAt = static_cast<double> (v.getProperty ("expires_at", 0)); }
    }
}
bool Tone3000::protect (const juce::MemoryBlock& input, juce::MemoryBlock& output)
{
#if JUCE_WINDOWS
    DATA_BLOB in { static_cast<DWORD> (input.getSize()), static_cast<BYTE*> (const_cast<void*> (input.getData())) }, out {};
    if (! CryptProtectData (&in, L"AVRX TONE3000", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) return false;
    output.replaceAll (out.pbData, out.cbData); LocalFree (out.pbData); return true;
#else
    juce::ignoreUnused (input, output); return false;
#endif
}
bool Tone3000::unprotect (const juce::MemoryBlock& input, juce::MemoryBlock& output)
{
#if JUCE_WINDOWS
    DATA_BLOB in { static_cast<DWORD> (input.getSize()), static_cast<BYTE*> (const_cast<void*> (input.getData())) }, out {};
    if (! CryptUnprotectData (&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) return false;
    output.replaceAll (out.pbData, out.cbData); LocalFree (out.pbData); return true;
#else
    juce::ignoreUnused (input, output); return false;
#endif
}
void Tone3000::store()
{
    if (statePath == juce::File()) return;
    juce::DynamicObject::Ptr o = new juce::DynamicObject;
    { const juce::ScopedLock guard (lock); o->setProperty ("client_id", key); o->setProperty ("access_token", access); o->setProperty ("refresh_token", refreshToken); o->setProperty ("expires_at", expiresAt); }
    const auto data = juce::JSON::toString (juce::var (o.get())); juce::MemoryBlock clear (data.toRawUTF8(), data.getNumBytesAsUTF8()), encrypted;
    if (! protect (clear, encrypted)) { message ("Connection works for this session; Windows could not securely save it."); return; }
    juce::TemporaryFile temp (statePath);
    if (statePath.getParentDirectory().createDirectory().wasOk() && temp.getFile().replaceWithData (encrypted.getData(), encrypted.getSize())) temp.overwriteTargetFileWithTemporary();
}
juce::String Tone3000::form (std::initializer_list<std::pair<juce::String, juce::String>> fields)
{
    juce::String result; for (const auto& f : fields) { if (result.isNotEmpty()) result += "&"; result += juce::URL::addEscapeChars (f.first, true) + "=" + juce::URL::addEscapeChars (f.second, true); } return result;
}
juce::String Tone3000::randomCode()
{
    std::array<unsigned char, 32> bytes {};
#if JUCE_WINDOWS
    if (BCryptGenRandom (nullptr, bytes.data(), static_cast<ULONG> (bytes.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) throw std::runtime_error ("Windows could not start a secure connection.");
#else
    throw std::runtime_error ("TONE3000 connection currently requires Windows.");
#endif
    return juce::String::toHexString (bytes.data(), static_cast<int> (bytes.size()), 0);
}
juce::String Tone3000::challenge (const juce::String& verifier)
{
    const auto raw = juce::SHA256 (verifier.toRawUTF8(), verifier.getNumBytesAsUTF8()).getRawData();
    return juce::Base64::toBase64 (raw.getData(), raw.getSize()).replaceCharacter ('+', '-').replaceCharacter ('/', '_').removeCharacters ("=");
}
juce::URL Tone3000::selectUrl (const juce::String& client, const juce::String& redirect, const juce::String& state, const juce::String& verifier)
{
    return juce::URL (api + "oauth/authorize").withParameter ("client_id", client).withParameter ("redirect_uri", redirect).withParameter ("response_type", "code").withParameter ("code_challenge", challenge (verifier)).withParameter ("code_challenge_method", "S256").withParameter ("state", state).withParameter ("prompt", "select_tone").withParameter ("format", "nam").withParameter ("gears", "amp-cab").withParameter ("architecture", "1").withParameter ("preview", "true").withParameter ("menubar", "true");
}
juce::StringPairArray Tone3000::callback (const juce::String& target, const juce::String& state)
{
    if (! target.startsWith ("/callback?")) throw std::runtime_error ("Invalid callback path.");
    const juce::URL u ("http://127.0.0.1" + target); juce::StringPairArray p;
    for (int n = 0; n < u.getParameterNames().size(); ++n) { const auto name = u.getParameterNames()[n]; if (p.containsKey (name)) throw std::runtime_error ("Duplicate callback fields."); p.set (name, u.getParameterValues()[n]); }
    if (p["state"] != state || state.isEmpty()) throw std::runtime_error ("Callback state did not match. Reconnect.");
    if (p["error"].isEmpty() && p["canceled"] != "true" && (p["code"].isEmpty() || ! p["tone_id"].containsOnly ("0123456789") || p["tone_id"].getLargeIntValue() <= 0)) throw std::runtime_error ("No tone was selected. Try again.");
    return p;
}
bool Tone3000::start (std::function<void()> task)
{
    if (busy.exchange (true)) return false; wait(); canceled.store (false);
    worker = std::thread ([this, task = std::move (task)] { try { task(); } catch (const std::exception& e) { message (canceled.load() ? "TONE3000 action canceled. Current capture retained." : e.what()); } busy.store (false); }); return true;
}
bool Tone3000::browse() { if (! validKey (getKey())) { message ("Create your TONE3000 account, then paste a publishable key from Settings / API Keys."); return false; } return start ([this] { select(); }); }
Tone3000::Reply Tone3000::request (const juce::String& address, const juce::String& body, const juce::String& headers, size_t maxBytes)
{
    if (canceled.load()) throw std::runtime_error ("Canceled");
    if (transport) { auto r = transport (address, body, headers); if (r.body.getSize() > maxBytes) throw std::runtime_error ("TONE3000 response too large."); return r; }
    Reply reply; juce::URL url (address); if (url.getScheme() != "https") throw std::runtime_error ("Use HTTPS for TONE3000 requests.");
    if (body.isNotEmpty()) url = url.withPOSTData (body);
    const auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress).withConnectionTimeoutMs (8000).withExtraHeaders (headers).withStatusCode (&reply.status).withResponseHeaders (&reply.headers).withNumRedirectsToFollow (0).withHttpRequestCmd (body.isEmpty() ? "GET" : "POST");
    auto stream = url.createInputStream (options); if (! stream) throw std::runtime_error ("Could not reach TONE3000. Check your internet connection.");
    std::array<char, 16384> buffer {};
    while (! stream->isExhausted()) { if (canceled.load()) throw std::runtime_error ("Canceled"); const int read = stream->read (buffer.data(), static_cast<int> (buffer.size())); if (read <= 0) break; if (reply.body.getSize() + static_cast<size_t> (read) > maxBytes) throw std::runtime_error ("TONE3000 response exceeds supported size."); reply.body.append (buffer.data(), static_cast<size_t> (read)); }
    return reply;
}
void Tone3000::tokens (const juce::var& v)
{
    const auto token = text (v, "access_token"), next = text (v, "refresh_token"); const double life = static_cast<double> (v.getProperty ("expires_in", 0));
    if (token.isEmpty() || next.isEmpty() || ! std::isfinite (life) || life <= 0 || token.containsAnyOf ("\r\n") || next.containsAnyOf ("\r\n")) throw std::runtime_error ("Invalid sign-in response. Reconnect.");
    { const juce::ScopedLock guard (lock); access = token; refreshToken = next; expiresAt = juce::Time::getCurrentTime().toMilliseconds() * .001 + life; }
    store();
}
void Tone3000::refresh()
{
    juce::String refreshValue; { const juce::ScopedLock guard (lock); refreshValue = refreshToken; }
    if (refreshValue.isEmpty()) throw std::runtime_error ("Browse TONE3000 to reconnect.");
    const auto r = request (api + "oauth/token", form ({ { "grant_type", "refresh_token" }, { "refresh_token", refreshValue }, { "client_id", getKey() } }), "Content-Type: application/x-www-form-urlencoded\r\n");
    if (r.status == 400 || r.status == 401) { { const juce::ScopedLock guard (lock); access.clear(); refreshToken.clear(); expiresAt = 0; } store(); throw std::runtime_error ("Your TONE3000 session expired. Browse to reconnect."); }
    requireReply (r); tokens (parse (r));
}
juce::var Tone3000::jsonRequest (const juce::String& path, const juce::String& body, bool authenticated)
{
    if (authenticated && juce::Time::getCurrentTime().toMilliseconds() * .001 > expiresAt - 60) refresh();
    auto headers = [this, authenticated] { const juce::ScopedLock guard (lock); return authenticated ? "Authorization: Bearer " + access + "\r\n" : juce::String ("Content-Type: application/x-www-form-urlencoded\r\n"); };
    auto r = request (api + path, body, headers());
    if (authenticated && r.status == 401) { refresh(); r = request (api + path, body, headers()); }
    requireReply (r); return parse (r);
}
std::vector<Tone3000::Model> Tone3000::parseModels (const juce::var& v)
{
    std::vector<Model> result;
    const auto data = v.getProperty ("data", {});
    if (auto* list = data.getArray()) for (const auto& m : *list)
    {
        Model model { text (m, "id"), text (m, "name"), text (m, "model_url") };
        const auto architecture = text (m, "architecture_version");
        if (model.id.containsOnly ("0123456789") && model.id.getLargeIntValue() > 0 && toneHost (juce::URL (model.url)) && (architecture.isEmpty() || architecture == "1")) result.push_back (model);
    }
    return result;
}
void Tone3000::select()
{
    const auto verifier = randomCode(), state = randomCode(); juce::StreamingSocket listener;
    if (! listener.createListener (0, "127.0.0.1")) throw std::runtime_error ("Could not open the local sign-in callback. Try again.");
    const auto redirect = "http://127.0.0.1:" + juce::String (listener.getBoundPort()) + "/callback";
    const auto url = selectUrl (getKey(), redirect, state, verifier);
    message ("Choose a NAM Amp + Cab capture in your browser. AVRX is waiting for your selection...");
    if (! (openBrowser ? openBrowser (url) : url.launchInDefaultBrowser())) throw std::runtime_error ("Could not open your browser.");
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + (transport ? 10000 : 300000); juce::StringPairArray fields; bool received = false; juce::String callbackError;
    if (transport && testCallback) { fields = callback (testCallback (url), state); received = true; }
    while (! received && ! canceled.load() && juce::Time::getMillisecondCounterHiRes() < deadline)
    {
        if (listener.waitUntilReady (true, 100) <= 0) continue;
        std::unique_ptr<juce::StreamingSocket> socket (listener.waitForNextConnection()); if (! socket) continue;
        juce::String requestText; const auto readDeadline = juce::Time::getMillisecondCounterHiRes() + 2000;
        while (! canceled.load() && requestText.length() < 8192 && ! requestText.contains ("\r\n\r\n") && juce::Time::getMillisecondCounterHiRes() < readDeadline)
        { if (socket->waitUntilReady (true, 100) <= 0) continue; std::array<char, 2048> bytes {}; const int n = socket->read (bytes.data(), static_cast<int> (bytes.size()), false); if (n <= 0) break; requestText += juce::String::fromUTF8 (bytes.data(), n); }
        const auto firstLine = requestText.upToFirstOccurrenceOf ("\r\n", false, false); const auto target = firstLine.fromFirstOccurrenceOf ("GET ", false, false).upToFirstOccurrenceOf (" HTTP/", false, false);
        try { if (! firstLine.startsWith ("GET ")) throw std::runtime_error ("Invalid method"); fields = callback (target, state); received = true; }
        catch (const std::exception& e) { callbackError = e.what(); const juce::String bad = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"; socket->write (bad.toRawUTF8(), static_cast<int> (bad.getNumBytesAsUTF8())); continue; }
        const juce::String content = "<html><body style='background:#12171c;color:white;font-family:sans-serif;padding:40px'><h2>Tracksmith Groove</h2><p>Return to Tracksmith Groove to choose and load your capture. You can close this tab.</p></body></html>";
        const juce::String response = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nCache-Control: no-store\r\nContent-Length: " + juce::String (content.getNumBytesAsUTF8()) + "\r\nConnection: close\r\n\r\n" + content; socket->write (response.toRawUTF8(), static_cast<int> (response.getNumBytesAsUTF8())); break;
    }
    if (canceled.load()) throw std::runtime_error ("Canceled"); if (! received) throw std::runtime_error (callbackError.isNotEmpty() ? callbackError.toStdString() : "Sign-in timed out. Browse TONE3000 to try again.");
    if (fields["error"].isNotEmpty()) throw std::runtime_error ("TONE3000 sign-in was declined or failed. Browse to try again.");
    if (fields["code"].isNotEmpty()) tokens (jsonRequest ("oauth/token", form ({ { "grant_type", "authorization_code" }, { "code", fields["code"] }, { "code_verifier", verifier }, { "redirect_uri", redirect }, { "client_id", getKey() } })));
    if (fields["canceled"] == "true") { message ("Selection canceled. Your current capture is unchanged."); return; }
    message ("Fetching the selected tone and its models...");
    const auto tone = jsonRequest ("tones/" + fields["tone_id"] + "?architecture=1", {}, true);
    auto found = parseModels (jsonRequest ("models?tone_id=" + fields["tone_id"] + "&architecture=1&page_size=300", {}, true));
    if (found.empty()) throw std::runtime_error ("This tone has no supported A1 NAM models. Choose another Amp + Cab capture.");
    { const juce::ScopedLock guard (lock); available = std::move (found); selectedTone = fields["tone_id"]; selectedTitle = text (tone, "title"); selectedCreator = text (tone.getProperty ("user", {}), "username"); selectedUrl = text (tone, "url"); if (! toneHost (juce::URL (selectedUrl))) selectedUrl = "https://www.tone3000.com/"; }
    modelRevision.fetch_add (1); message ("Choose a model below, then Download & load. Your current rig remains active until loading succeeds.");
}
bool Tone3000::download (int index)
{
    Model chosen; { const juce::ScopedLock guard (lock); if (index < 0 || index >= static_cast<int> (available.size())) return false; chosen = available[static_cast<size_t> (index)]; }
    return start ([this, chosen]
    {
        if (cacheFolder == juce::File() || ! cacheFolder.createDirectory().wasOk()) throw std::runtime_error ("Could not create the local TONE3000 capture folder.");
        message ("Downloading " + chosen.name + "...");
        if (juce::Time::getCurrentTime().toMilliseconds() * .001 > expiresAt - 60) refresh();
        juce::String bearer; { const juce::ScopedLock guard (lock); bearer = "Authorization: Bearer " + access + "\r\n"; }
        auto r = request (chosen.url, {}, bearer, 32 * 1024 * 1024);
        if (r.status == 401) { refresh(); { const juce::ScopedLock guard (lock); bearer = "Authorization: Bearer " + access + "\r\n"; } r = request (chosen.url, {}, bearer, 32 * 1024 * 1024); }
        for (int n = 0; n < 4 && r.status >= 300 && r.status < 400; ++n)
        {
            const juce::URL destination (r.headers["Location"]); if (destination.getScheme() != "https") throw std::runtime_error ("The model download redirected to an unsupported address.");
            r = request (destination.toString (true), {}, toneHost (destination) ? bearer : juce::String(), 32 * 1024 * 1024);
        }
        requireReply (r); const auto capture = parse (r);
        if (! capture.hasProperty ("architecture") || ! capture.hasProperty ("weights")) throw std::runtime_error ("Downloaded file is not a NAM capture. Previous amp retained.");
        if (canceled.load()) throw std::runtime_error ("Canceled");
        const auto file = cacheFolder.getChildFile ("TONE3000-" + chosen.id + "-" + juce::File::createLegalFileName (chosen.name).substring (0, 100) + ".nam").getNonexistentSibling();
        if (! file.replaceWithData (r.body.getData(), r.body.getSize())) throw std::runtime_error ("Could not save the capture locally.");
        juce::DynamicObject::Ptr info = new juce::DynamicObject; info->setProperty ("title", title()); info->setProperty ("creator", creator()); info->setProperty ("url", toneUrl()); info->setProperty ("model", chosen.name); info->setProperty ("id", chosen.id);
        file.withFileExtension ("tone3000.json").replaceWithText (juce::JSON::toString (juce::var (info.get())));
        { const juce::ScopedLock guard (lock); readyFile = file; }
        message ("Downloaded " + chosen.name + ". Loading into your NAM block...");
    });
}
juce::File Tone3000::takeReady() { const juce::ScopedLock guard (lock); auto result = readyFile; readyFile = {}; return result; }
void Tone3000::disconnect()
{
    if (busy.load()) { cancel(); return; }
    { const juce::ScopedLock guard (lock); access.clear(); refreshToken.clear(); expiresAt = 0; available.clear(); }
    store(); modelRevision.fetch_add (1); message ("Disconnected. Downloaded captures and your current rig are retained.");
}
}
