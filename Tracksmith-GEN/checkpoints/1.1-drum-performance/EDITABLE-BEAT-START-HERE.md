# Tracksmith GEN 0.8.5 - editable beat foundation

Run ALIGN-DRUMS.cmd. Choose your original guitar PCM WAV and the drum settings, then recording tempo and first drum bar. GGD One Kit Wonder: Metal factory mapping, 140 BPM and bar 2 remain the defaults.

Default: Basic complexity, straight timing (50% swing), hi-hat, fills off, crashes off, and Follow strong guitar attacks on. This gives kick on 1/3, snare on 2/4, straight eighth-note timekeeping, and at most one extra kick per bar from strong detected guitar attacks. All notes remain ordinary editable MIDI on channel 10.

The attack guidance uses rising RMS energy, not a trained guitar-to-drums model. It chooses only strong attacks, snaps the extra kick to an eighth-note position, avoids snare beats, suppresses duplicate kicks, and limits additions to one per bar. It can miss muted or subtle guitar notes and does not understand phrasing or song sections. Turning it off gives the regular repeatable foundation.

Controls:
- Complexity: Basic; Developed adds a supporting kick; Fuller adds another alternating kick. Start Basic and edit in your DAW.
- Swing: 50% is straight; 55-58% is light; 66.7% is approximately triplet swing. Only offbeat closed/open hi-hat or ride hits are delayed. Kick, snare and tom-fill positions remain anchored, so swing cannot accumulate bar drift.
- Timekeeping cymbal: hi-hat or ride.
- Tom fills: off, or one short three-hit fill at the end of each complete 4- or 8-bar phrase. Fills are omitted when the source ends before that phrase is complete.
- Crashes: optional, at the start and every four bars.

IMPORT AT BAR 1. The first bar is silent when First drum bar is 2. Do not move the file to bar 2 as that doubles the offset. Your WAV should cover the timeline from bar 1; export it that way if necessary. Keep the DAW at the same recording tempo. A user-edited MIDI file can be freely changed after import; regenerate to a new filename to keep your edits intact.

Included demos: GGD-Simple-140-Bar2.mid is the straight basic reference. GGD-Swing-58-Full-Kit-140-Bar2.mid demonstrates 58% swing, more kick, crashes and short tom fills over eight bars. Both are already GGD-mapped and should be imported at bar 1 at 140 BPM. The swung demo is an option example, not the default level of complexity.

The main TracksmithGEN.exe audio-generation workflow is unchanged; use ALIGN-DRUMS.cmd for these MIDI controls. UPDATE-GEN.cmd copies the utility modules into your existing GEN folder and backs up replaced program files; saved takes/settings remain intact. You can also run from this extracted folder. No GPU or new model download is needed.

Validation: 13 automated tests passed, covering baseline timing/mapping, swing scope, triplet timing, fixed bar positions, optional kit voices, phrase fills, sparse onset guidance, duplicate suppression, partial endings and invalid input rejection. The demo was independently checked for bar-2 start, mapped voices and kick/snare alignment. PowerShell syntax was checked. Guitar fit still needs a listening check. The prior C# remapper is unchanged and its earlier runtime-test limitation still applies; no remapper test was retried.

The alignment JSON records the settings and candidate guitar accent kicks. GMD model attribution/license and earlier mapping documentation remain included.
