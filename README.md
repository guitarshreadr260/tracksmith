# Tracksmith

Private project reference for **Tracksmith DAW** and **Tracksmith Groove**, owned by guitarshreadr260.

## Start here while away from your PC

- [Where we left off and what's next](PROJECT-STATUS.md)
- [Build history](BUILD-HISTORY.md)
- [How to use this GitHub repository](WORKING-AWAY-FROM-PC.md)
- [Download the DAW automation build](Tracksmith-Windows-Automation.zip)
- [Groove plugin downloads](../../releases)

This first upload is a reference collection with source snapshots, guides and tested builds. Source snapshots are ZIP archives; download and extract them to browse or build locally. They are not yet a normal, fully expanded Git working tree. Updates appear here when we publish them; this does not automatically mirror the home PC or its live chat.

## Current work

**Tracksmith DAW:** Windows audio/MIDI DAW with native audio processing, recording and monitoring, retake replacement, fades, track playlists, VST3 inserts, aux/bus routing, WAV export and volume/pan automation. Custom UI work is deferred until the core workflow is further along.

**Tracksmith Groove 0.18:** The preserved AVRX Groove 0.17 studio renamed and converted to a separate VST3 instrument. Includes the supplied kits and groove library in its release bundle. It follows the DAW transport/tempo, accepts MIDI and saves kit/pattern settings in the DAW session. Groove is optional; the DAW has no built-in drum engine.

## Guides

- [DAW automation](Tracksmith-Automation-Guide.md)
- [Track playlists, retakes and fades](Tracksmith-Takes-Playlists-Guide.md)
- [Mixer and aux/bus routing](Tracksmith-Mixer-Routing-Guide.md)
- [Recording and input monitoring](Tracksmith-Recording-Guide.md)
- [WAV export](Tracksmith-Export-Guide.md)
- [Plugin browser and New Tracks dialog](Tracksmith-Plugin-Browser-Guide.md)
- [Tracksmith Groove VST3](Tracksmith-Groove-VST3-Guide.md)

## Source snapshots

- [Tracksmith DAW automation source](Tracksmith-Source-Automation.zip)
- [Tracksmith Groove 0.18 code](Tracksmith-Groove-Source-Code-v0.18.zip)

The Groove source archive excludes the large supplied audio/MIDI/capture libraries. They are included inside the VST3 release bundle under Contents/Resources. To build Groove with these libraries, copy Drum Packs, Grooves and Captures from that folder into the extracted source folder. External JUCE/toolchain setup is still required by the CMake projects. Repository visibility is private; no public/open-source license has been assigned to the project's own code. Third-party components retain their supplied licenses.

## Validation

See the validation files for the automated checks and their limits. These checks do not replace listening and hardware tests on the user's interface. Keep copies of older sessions: new DAW automation saves use format 7 and cannot be opened by older DAW builds.
