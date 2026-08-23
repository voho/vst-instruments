# Septum presets

The plug-in ships 64 programs built into the binary
(`Source/DSP/SeptumPresets.cpp`), laid out the way the instrument lays out its
own bank: 32 original sounds in the PRESET A-1…D-8 positions, then 32
initialised User slots. They are original sounds programmed against the
engine — the modelled instrument's own 64 factory patches are Roland's data,
published nowhere as parameter values, and none of that data ships in this
repository. An initialised patch reproduces the documented initialization
behavior: only OSC 1 is heard, because the balance sits fully left.

Host sessions store the full parameter state, so any edited sound saves with
the project.

A SysEx dump of a real-hardware patch can be loaded: send it to the plug-in as
a MIDI System Exclusive message and it becomes the current patch. The codec
reads the address map's 22 blocks — the patch common block, both tones, the
delay and reverb, the arpeggio's own block and the sixteen pattern blocks that
hold its 32 × 16 grid. All of it survives the trip on into the plug-in: the
whole parameter surface including the full 5–300 BPM tempo, and the imported
grid, which has no parameter to live in and is kept beside them. That grid
plays in place of the selected style template until the selector is moved, is
saved with the session, and comes back out of a re-export. Only the patch's
**name** is dropped. The plug-in transmits no SysEx of its own and rejects RQ1
data requests; see the README's inventory.
