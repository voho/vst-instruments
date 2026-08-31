Shinyguitar acoustic-microphone sustain source

Work: Shinyguitar archtop-guitar sample library
Performer and mapping: D. Smolken / Karoryfer Lecolds
Source: https://github.com/sfzinstruments/karoryfer.shinyguitar
Pinned commit: 57243cca85277dbcc120ce17c6178032f93c80f3
Licence: CC0 1.0 Universal (see CC0-1.0.txt)

Acustra embeds the 272 acoustic-microphone sustain files selected by
Programs/acoustic.sfz: 17 pitch roots, four MIDI-velocity layers and four
round robins. Only files ending `_vl[1-4]_rr[1-4]_1.wav` are included; release
noises and magnetic-pickup (`_2.wav`) signals are not included.

Aggregate SHA-256:
4526e15b6b242dd68481c48eacae9409c09bc3bebec1da39cf66070f36a4ef7c

Aggregate method: sort included paths relative to the repository root by their
UTF-8 POSIX spelling, then hash each path, one NUL byte, and the file bytes in
that order.

The full 44.1-kHz mono sustain durations and captured relative levels are kept.
Generation converts 24-bit WAV to signed PCM16 and applies only a 60 ms
terminal half-cosine de-click fade before lossless Rice-delta packing. Runtime
uses the SFZ key and velocity ranges and deterministic four-way round robin.
Each playback root is the H1 spectral peak measured over a two-second Hann
window that begins 820 ms after the detected onset. A search-boundary fit falls
back to the median of valid round robins in its root/layer, preserving the
captured attack glide without accepting an unstable low-note estimate.
