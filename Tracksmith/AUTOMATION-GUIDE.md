# Tracksmith — volume and pan automation

Save and close the old build. Extract the whole Windows ZIP, then open Tracksmith.exe. Stock Plugins stays beside the executable. Plugin scanning has been left as it was in the previous build for later work. Drums remain excluded and AVRX Groove remains saved separately.

## Open automation

Select an audio, MIDI / Instrument, or Aux / Bus track. In the left Inspector, scroll to Automation and turn Volume / Pan lanes ON. The editor appears above the arrangement or mixer. Each mixer strip also has an Automation mode selector.

- Off uses the manual volume and pan controls and preserves any saved curves.
- Read follows the saved curves. The faders display the current curve values and are locked against manual changes; select Off or Write to move them. Empty lanes use the track's manual value.
- Write records volume and pan movement while the transport plays. Turn Loop off, stop playback, choose Write for the tracks you want to automate, press Play, and move their normal volume and pan controls. Press Stop to commit the passes. These tracks automatically return to Read. You do not need to press the audio Record button.

Modes can be changed while stopped. Automation is shared by a track's playlists. Volume affects its post-fader sends, and bus automation controls the stereo return. Audio recording continues to capture dry input.

## Edit a curve

Choose Volume or Pan. Click the curve to add a point at that time and value. Click a dot or choose a Saved point to select it, then change Value and click Add / replace point to update it at the same time. To move a point in time, delete the old point and add a new one. Use playhead copies the current position into Time; it does not add a point until you click Add / replace point. Delete point removes that point. Clear lane removes only the selected volume or pan curve.

Volume values are linear gain: 0 is silent and 1 is unity. Pan ranges from -1 (left) through 0 (center) to +1 (right). Curves interpolate linearly and hold the first or last value outside their points. Fader changes retain the engine's short smoothing to avoid clicks. The first automation point therefore also defines the sound before that point; add a point at zero when you want an initial value.

Undo automation and Redo automation apply to automation edits and committed Write passes, independently of clip/MIDI undo. One stopped Write pass groups its recorded tracks into an undo step. Undo restores the preceding curves, mode and manual fader values. Up to 32 recent steps are retained, with a memory cap for large sessions. Histories are not saved in the session.

## Write behavior and limits

Writing replaces the traversed time range on both volume and pan lanes of each Write track, preserving the old curves immediately before and after that range. Leaving a fader still writes its held value. Keep a session copy if you want to compare complete passes. Seeking during a Write pass commits it and stops writing; reselect Write while stopped to start another pass. Saving or exporting commits any active pass first.

Capture uses the shared native transport clock, updated at control events and up to 20 times a second for held values; playback evaluates the resulting curve for each output sample. Constant and collinear sections are reduced to retain long holds. Each lane is limited to 4096 points over at most 24 hours. If a track's pass exceeds the limit, its previous curves are retained and the status reports the issue. Explicit point edits also respect that limit.

Loop writing, Touch/Latch modes, master/send automation, hardware controller writing, mute automation, and plugin parameter automation are future work. You can loop playback in Read. This build does not automate native plugin editor knobs.

## Saving and exporting

Automation and modes are saved in session version 7. Write is saved as Read to avoid accidentally overwriting a curve after reopening. This build opens session versions 1–7; older sessions default to Off with no curves. Older Tracksmith builds cannot open version 7 saves, so keep a copy of important version 6 sessions before saving them here.

Full-session and selection WAV exports include enabled automation at absolute session times, including source and bus plugin latency compensation. Off exports the manual mix. An armed but idle Write mode reads its saved curve for export without writing new points.

## Validation

Native curve, capture, overwrite preservation, limits, playback, seeking, mode, mute, bus/send, session and WAV sample tests pass. Real VST3 source/bus/master delay tests compare automated exports sample-for-sample against a graph without latency. TypeScript and existing keyboard shortcut tests pass. Final hardware listening, fader recording gestures and interface click-through remain for the user to check.
