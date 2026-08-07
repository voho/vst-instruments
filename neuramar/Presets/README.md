# Neuramar learned instruments and presets

Neuramar creates an instrument by analysing a user-selected audio sample,
identifying its root pitch, and learning a compact synthesis model. The source
sample is user content and is not part of this repository.

The host-state contract stores the ordinary sound controls (18 host
parameters; sessions saved by version 1.1 carry 17), root-note metadata,
display-only source name and waveform preview, and the versioned compact model
needed to reproduce the instrument. The current model payload is version 5,
which widens the body representation from 8 Air bands and 6 Bone modes to 16
and 12; version-2, version-3, and version-4 payloads written by earlier
releases still load and render unchanged, and controls added after a session
was saved are restored at the values that reproduce its original sound rather
than inheriting whatever
the running instance held. It does not package or redistribute the
original source recording. Exported presets carry the Neuramar state/model
version so future migrations remain explicit.

Factory presets may be added only when every included setting, learned model,
and source-derived asset is original or cleared for redistribution. Prefer
descriptive names based on sound character rather than a named artist,
performer, protected recording, or existing commercial instrument.

No factory samples, pretrained weights, or third-party preset library are
included in version 1.1.
