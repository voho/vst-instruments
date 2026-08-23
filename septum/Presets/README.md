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
a MIDI System Exclusive message and it becomes the current patch. What comes
back is the whole parameter surface — both tones, the patch common block, the
delay and reverb, the arpeggio's settings and the full 5–300 BPM tempo — but
not the arpeggio's 32 × 16 grid or the patch's name, neither of which has a
plug-in parameter to live in. The plug-in transmits no SysEx of its own and
rejects RQ1 data requests; see the README's inventory.
