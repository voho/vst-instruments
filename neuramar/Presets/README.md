# Neuramar learned instruments and presets

Neuramar creates an instrument by analysing a user-selected audio sample,
identifying its root pitch, and learning a compact synthesis model. The source
sample is user content and is not part of this repository.

The host-state contract stores the ordinary sound controls, root-note metadata,
display-only source name and waveform preview, and the versioned compact model
needed to reproduce the instrument. It does not package or redistribute the
original source recording. Exported presets carry the Neuramar state/model
version so future migrations remain explicit.

Factory presets may be added only when every included setting, learned model,
and source-derived asset is original or cleared for redistribution. Prefer
descriptive names based on sound character rather than a named artist,
performer, protected recording, or existing commercial instrument.

No factory samples, pretrained weights, or third-party preset library are
included in version 1.0.
