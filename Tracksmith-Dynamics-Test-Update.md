# Tracksmith stock dynamics development

Gate and Bus Compressor development test notes. These plugins are now included in the combined stock-plugin test build; see Tracksmith-Stock-Plugins-Guide.md.

## Gate — passed
- Above-threshold opening and below-threshold attenuation to the selected range.
- Hysteresis, hold expiry, bypass unity and silence.
- Linked stereo gain and no channel crosstalk.
- 8, 44.1, 48, 96 and 192 kHz processing.
- Actual VST3 discovery/loading, parameter state round trip and zero reported latency.
- Native editor rendered and visually inspected.

## Bus Compressor — passed
- 4:1 static curve and measured steady-state attenuation.
- Makeup gain, stereo gain tracking, fully dry mix and bypass unity.
- Silence and recovery after compression.
- Auto release retains a longer recovery after sustained compression than after a brief peak.
- Sidechain high-pass rejects DC in the detector without removing DC from the audio path.
- 8, 44.1, 48, 96 and 192 kHz processing.
- Actual VST3 discovery/loading, parameter state round trip and zero reported latency.
- Native editor rendered and visually inspected.

The compressor is an original feed-forward soft-knee design. Current controls use continuous ranges. Its dual-time Auto release includes a slower accumulator for sustained signals. This is not yet a circuit-level GSSL/SSL model or an exact SSL sonic match. Oversampling and external sidechain routing are not implemented in this first version. Listening tests and broader transient/automation measurements remain before a release build.

References reviewed:
- https://www.solidstatelogic.com/assets/uploads/downloads/support.solidstatelogic.com_Bus_Compressor_2_User_Guide.pdf
- https://gyraf.dk/gy_pd/ssl/ssl.htm

The Gyraf page was reviewed for architectural context. Its schematics, PCB artwork and project implementation were not copied into these plugins.

## FET Compressor — development checks passed
- Four standard ratios (4, 8, 12, 20) plus custom Slam mode reach the expected steady-state attenuation.
- Input drive increases gain reduction; stereo channels retain the same gain ratio.
- Faster attack responds sooner than slower attack.
- Dry mix and settled bypass return original audio. Silence recovers without producing signal.
- DSP checked at 8, 44.1, 48, 96 and 192 kHz.
- Actual VST3 discovery/loading, saved parameter recall, zero latency and native editor rendering passed.

This is an original behavioral fast compressor, not a transistor-level FET model. Hardware nonlinearities, transformer coloration, oversampling and hardware-matching have not been implemented or validated. Attack/release labels specify control-loop constants; response also depends on ratio and signal. Slam is a custom 40:1/lower-threshold mode, not a validated model of hardware all-buttons operation.

## Limiter — development checks passed
- Sample peaks stay within the selected ceiling while drive and ceiling change.
- Stereo gain linking, settled bypass and no crosstalk.
- Five sample rates from 8 to 192 kHz.
- Actual VST3 discovery/loading, parameter state recall, zero latency and native editor rendering.

The limiter controls sample peaks only. It does not currently measure or guarantee intersample true peaks, and uses neither lookahead nor oversampling. The submitted circuit page returned a verification screen, so its circuit was not inspected or reproduced. This is an independent digital implementation.

## De-Esser — development checks passed
- A 6 kHz test tone is reduced by over 5 dB; a 200 Hz test tone changes by less than 0.1 dB with default Auto threshold settings.
- Auto mode produces reduction within 0.1 dB across a 20 dB change in test-tone input level.
- Manual threshold triggers reduction.
- Stereo linking, settled bypass and finite output during frequency/listen/bypass changes.
- Five sample rates from 8 to 192 kHz.
- Actual VST3 discovery/loading, parameter state recall, zero latency and native editor rendering.

This is selective band cancellation with a manual centre frequency and adaptive threshold, not SPL's proprietary scanning system. The processing frequency is clamped below Nyquist at low sample rates. Listening tests on vocals and broader transient/automation measurements remain.

Additional references reviewed:
- https://www.gyraf.dk/gy_pd/1176/1176.htm
- https://media.uaudio.com/assetlibrary/2/-/2-1176_manual.pdf
- https://spl.audio/wp-content/uploads/9629_OM_E.pdf
- https://www.circuits-diy.com/audio-limiter-circuit/ (unavailable behind verification screen)

The DAW insert-menu source compiles with entries for the EQ and five stock dynamics plugins. The combined portable stock-plugin test build now includes these plugins; no updated installer was created.

