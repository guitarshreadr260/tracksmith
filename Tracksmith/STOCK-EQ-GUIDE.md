# Tracksmith EQ 7

Extract the entire Tracksmith-Windows-Stock-EQ-7.zip to a new folder. Run Tracksmith.exe from that folder. Keep Stock Plugins alongside it. This portable build can be used alongside your installed version; close the other Tracksmith before opening a session.

On a track or the master, open an insert menu and choose **Add stock Tracksmith EQ 7**. Open its plugin editor to adjust it. On an instrument track, place effects after the instrument in inserts B–E.

Seven bands: high-pass/notch, low shelf/bell, three bell bands, high shelf/bell, low-pass/notch. Pass filters have 6, 12, 18 or 24 dB/octave slopes. Gain bands and input/output trims cover ±18 dB. Frequency range is 20 Hz–20 kHz, limited internally near Nyquist at low sample rates.

Drag a graph dot for frequency and gain. Shift-drag or use the wheel for Q. Double-click a graph dot to enable/disable that band. Type exact values under knobs; double-click a knob to reset it. Pass-filter Q is fixed for the selected Butterworth slope; Q is adjustable in notch mode.

Reset flat restores all defaults. High/low pass are off by default. Input/output peak meters fall to silence when no signal is present. Red indicates overload; reduce output trim when boosting. Bypass returns the original signal, including bypassing trims/polarity. Processing adds zero latency, supports mono/stereo, saves with sessions and exposes parameters to existing Tracksmith plugin automation.

The separate VST3 zip contains only the EQ for manual installation in another Windows VST3 host. The complete portable build is the easiest way to try it in Tracksmith.

This is an original Tracksmith EQ using a layout inspired by Avid EQ III; it does not claim to match Avid's exact sound or artwork. Band reference: https://apps.avid.com/proToolsFirstHelp/version12.0/enu/Pro%20Tools%20First%20Help/EQ3.41.28.html
Filter formula reference: https://www.w3.org/TR/audio-eq-cookbook/

Validation: Release build and actual VST3 loading/state round trip passed. Tests cover bell gain, pass cutoff and slopes, shelves, notch depth, rapid extreme parameter changes at 8/44.1/48/96/192 kHz, stereo isolation, flat unity, bypass, polarity, silence, and native editor creation/render. Existing VST3 hosting, monitoring, delay compensation, automation, routing, clip/session and WAV export suites passed. Listening with your interface remains the next user check.
