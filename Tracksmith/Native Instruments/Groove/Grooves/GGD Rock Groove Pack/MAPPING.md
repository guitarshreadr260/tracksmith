# GGD Rock Groove Pack in AVRX

All 225 user-supplied original MIDI files are copied unchanged across Songs 01–15. The AVRX manifest selects GGD Groove Player/MIDI Pack mapping (different from the instrument's Invasion / GM map), with song BPM and meter from the folder labels. Some MIDI meta tags contain 85 BPM / 4/4 even in differently labelled folders; the manifest overrides these during import without editing the MIDI bytes. Song 03 uses 200 BPM, 6/8.

The 9-pad AVRX kit approximates GGD's larger articulation set:

| Source notes | AVRX destination |
|---|---|
| 36 | Kick |
| 37, 38, 39, 69 | Snare (stick / ruff / flam approximated as a single hit) |
| 50 | Rack Tom 1 |
| 48 | Rack Tom 2 |
| 43, 41 | Floor Tom (both floor toms use the single pad) |
| 10, 24, 42, 44, 54, 62 | Closed Hat (tip / edge / pedal collapsed) |
| 5, 25, 46, 58, 60, 70, 56 | Open Hat (openness / aux hat collapsed) |
| 49, 52, 55, 57, 64, 74, 75 | Crash (crash / china / splash / stack collapsed) |
| 51, 53, 59 | Ride (bow / bell / edge collapsed) |
| 66, 95 | Choke commands ignored; no extra crash hit |
| Other notes | Ignored and counted in import status |

Note 0 occurs once in the supplied pack and is ignored. Velocities are retained except coincident hits merged on the same pad/grid cell, where the stronger hit wins. Timing snaps to the imported grid, as with other AVRX MIDI imports. Feel adds the app's optional variation; set Feel to zero to keep imported velocities/grid exact. Original MIDI remains available for using its full articulation/timing data elsewhere.

Mapping reference: https://drumverter.com/engines/ggd-grooveplayer/ (the converter author's public note chart). Format distinction: https://www.midiremap.com/faq. No Kontakt instruments or GGD sample audio are copied.
