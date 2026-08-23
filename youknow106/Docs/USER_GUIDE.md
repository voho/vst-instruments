# YouKnow106 user guide

YouKnow106 1.1.0 is a six-voice software synthesizer from Protocodus. It runs
on macOS 11 or later as a universal `arm64`/`x86_64` VST3, Audio Unit, and
standalone application.

## Install

Download the signed and notarized `YouKnow106-1.1.0-macOS-universal.pkg`,
open it, and follow the macOS Installer prompts. The package installs:

- VST3: `/Library/Audio/Plug-Ins/VST3/YouKnow106.vst3`
- Audio Unit: `/Library/Audio/Plug-Ins/Components/YouKnow106.component`
- Standalone: `/Applications/YouKnow106.app`
- Documentation: `/Library/Application Support/Protocodus/YouKnow106/Documentation`

If the release includes its `SHA256SUMS.txt` file, verify the download from the
directory containing the PKG, manifest, and `SHA256SUMS.txt` files before
opening the installer:

```sh
shasum -a 256 -c YouKnow106-1.1.0-macOS-universal-SHA256SUMS.txt
```

Quit the DAW and standalone application before installing or upgrading.

Restart the DAW after installation so it rescans plug-ins. If the Audio Unit
does not appear, use the DAW's plug-in manager to rescan YouKnow106. The
standalone application is available in `/Applications` and lets you select its
audio and MIDI devices directly.

### Upgrading from a pre-v1 nightly

Pre-v1 public nightly builds used Pluto Audio bundle identifiers and the
package receipt `audio.youknow106.synth.pkg`. The first commercial release uses
the final Protocodus identifiers while preserving the Audio Unit and VST3 host
identities. Quit the DAW, install v1, then restart and rescan it. After the new
version is working, the obsolete receipt and documentation can be removed:

```sh
sudo pkgutil --forget audio.youknow106.synth.pkg
sudo rm -rf "/Library/Application Support/YouKnow106"
```

## First sound

Load YouKnow106 as a software instrument, choose a sound from PRESET
menu, and play MIDI notes. The panel follows the signal flow from LFO and DCO
through HPF, VCF, VCA, envelope, and chorus. Hover a control for a short
description and its current value.

The QUALITY menu selects 1x, 2x, or 4x internal processing. **New instances
use 1x**, which is the cheapest and aliases the most. Choose 4x for the
highest-quality offline render or a lightly loaded session — it is the setting
this project's own numerical-quality audits admit across every modelled domain
— and 2x as a middle position. A quality change waits until the instrument is
idle; at high host sample rates the effective factor may be reduced
automatically.

The VCF SOLVER menu beneath it sets how much arithmetic the filter's own
solver spends on each internal sample. It does not change the internal rate, so
it costs no latency and takes effect immediately rather than waiting for an
idle moment.

- **Normal** is what new instances use. It takes one solver step per internal
  sample wherever one step is numerically admissible, and automatically takes
  more where it is not — so a wide-open resonant filter still gets the work it
  needs. On the measured x86-64 reference machine it uses about half the CPU of
  Max.
- **High** costs about a fifth more than Normal, for the same order of accuracy
  at half the step size.
- **Max** is the most expensive, and is what every earlier release ran.

Hover the menu to read which numerical method each one uses.

All three run the same filter and keep the same resonance calibration and the
same self-oscillation amplitude and pitch. Asked to pick them apart in a blind
listening test — a resonant lead, a self-oscillation and sustained chords — the
player reported no audible difference between any of them, which is why
Standard is the shipped setting. QUALITY and VCF SOLVER are independent, so a
dense session can lower either or both. Neither is part of a patch, neither is
recalled by a program, and both persist with the session.

The factory bank is read-only. Host sessions and host presets retain edited
states. The PATCH FILE buttons load and save hardware-compatible `.syx` files.
RELOAD discards edits to the selected program, INIT restores the initial patch,
and PANIC clears held notes and sounding voices.

Keyboard users can use Tab and Shift-Tab to reach every enabled control; a
visible outline marks the current target. Arrow keys adjust sliders and menus,
and Space or Return activates buttons. On the performance lever, hold Left or
Right for pitch bend and Up for full modulation; releasing the arrow keys,
moving focus, or pressing Space, Return, or Escape returns both spring-loaded
axes to zero.

## MIDI and patch files

YouKnow106 responds to notes, pitch bend, modulation (CC 1), sustain (CC 64),
all-notes-off, and Program Change 0–127. The synthesis panel is available to
host automation but has no MIDI CC-learn mapping.

LOAD imports the first compatible patch dump from a `.syx` file; a file can
also be dropped on the editor. SAVE writes the current tone as a compatible
single-patch `.syx` dump. Performance and plug-in extension controls are not
part of that hardware-format tone data.

## Troubleshooting

- No plug-in in the DAW: restart the DAW, confirm that its plug-in format is
  installed at the path above, then request a rescan.
- No audio: confirm an instrument track is receiving MIDI and that the track,
  host, and interface outputs are active. In the standalone app, check the
  selected audio device.
- Stuck note: click PANIC or send MIDI all-notes-off.
- High CPU: new instances already start at the cheapest QUALITY and VCF SOLVER
  settings. If you have raised either and need the headroom back, lower VCF
  SOLVER first — it applies immediately and changes only the filter's numerical
  solve, not the internal rate the oscillators and chorus run at.
- A problem persists: include the YouKnow106 version, macOS version, Mac model,
  host and host version, plug-in format, sample rate, and reproduction steps in
  a message to [protocodus@proton.me](mailto:protocodus@proton.me).

## Uninstall

Quit all audio applications. Move the three YouKnow106 items listed under
Install to the Trash, then remove the documentation folder. An administrator
can instead remove those exact paths in Terminal:

```sh
sudo rm -rf "/Library/Audio/Plug-Ins/VST3/YouKnow106.vst3"
sudo rm -rf "/Library/Audio/Plug-Ins/Components/YouKnow106.component"
sudo rm -rf "/Applications/YouKnow106.app"
sudo rm -rf "/Library/Application Support/Protocodus/YouKnow106"
sudo pkgutil --forget cz.protocodus.youknow106.pkg
```

DAW projects may retain their own YouKnow106 state after uninstalling. The
standalone app may also leave its audio/MIDI device preferences at
`~/Library/Application Support/YouKnow106.settings` so a later installation can
reuse them. Remove either kind of user data only if it is no longer needed.

Privacy information, the source-code licence, third-party notices, and release
history are included with the installed documentation. Product information is at
[protocodus.cz](https://protocodus.cz).

YouKnow106 is an independent Protocodus product. It is not affiliated with,
endorsed by, sponsored by, or licensed by Roland Corporation. Roland and Juno
are trademarks of Roland Corporation and are used only to identify the
instrument architecture being modeled.
