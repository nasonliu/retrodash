# Peaceful Days — RetroDash SPC song project

The embedded RetroDash demo uses this source SPC, audio bank, melody chart and Chrono-themed artwork. See the repository README and docs/hardware-validation.md for hardware evidence.

- Source: user-supplied Chrono Trigger **Pre-Release** SPC; 151-second ID666 duration.
- Audio: four original BRR samples; eight S-DSP voices, original envelopes and echo.
- Chart: eighth-note merge grid, 200 manual targets and two automatic melody fills. Raw sound timing remains SPC-derived; pitch labels are calibrated against the supplied MIDI.
- Controls: KNOB1 changes visual scroll 50–200%; NEXT bright/steady, THEN dim/on-beat; accompaniment has no key hints.
- Structure: music/, audio/, chart/, art/, scene/, licenses/, verification/.
- Loading: compiled into the FM-1 firmware, not imported over USB in this beta. The compiler reads this project’s own art/scene files.
- Rights: original game music/characters remain with their respective owners; they are not relicensed under the firmware GPL.

The optional tools/ and vendor/ directories can reproduce the offline audio extraction/reference comparison. Full firmware builds use the root tools and firmware directories.

Creation guide: ../../docs/creating-song-projects.md
