# Build 33 - timeline audio references

Open portable/Start-Tracksmith-Generation.cmd, then Generate. With the panel open, drag an audio clip from the timeline into the reference box, or select the clip and click Analyze selected clip.

The worker copies up to the first 60 seconds of the trimmed source, applies clip fades, and measures duration, peak/RMS levels and estimated tempo/key. Track inserts and fader processing are excluded. Use tempo/key explicitly; estimates may be undetermined or inaccurate. This is not note transcription, stem separation or automatic accompaniment.

Enable reference-based variation, adjust influence, describe the desired result and generate. Audition before inserting on a separate track. The original audio and clip remain unchanged. Closing the panel restores normal clip drag editing.

Implementation: docked nonmodal generator; timeline drag payload and selected-clip fallback; asynchronous native reference worker with cancellation and session-revision guard; ACE-Step cover request with a temp WAV; reference provenance in generated manifests. Text-only generation remains available.

Reference selection is transient and cleared on session load. Temporary WAVs remain in the Windows temp folder so queued engine jobs can finish. Generated WAVs and manifests remain in the existing generation cache. The launcher uses the same installed local engine as Build 32.

Windows Release build and browser checks passed. Real local audio-conditioned generation and separate-track insertion passed. Musical quality and live hardware playback remain for user testing. Mac validation remains deferred. No GitHub push performed.