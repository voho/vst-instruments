# Mars analog-modeling research notes

Mars uses a practical hybrid selected from established virtual-analog literature:

- **Wave digital filters (WDF)**, introduced by Fettweis, preserve passivity and map analog networks into stable discrete scattering structures. They are a strong fit for diode/transistor ladder-style filters and one-pole RC stages because component tolerances can be represented directly while remaining robust at high resonance.
- **Zero-delay-feedback (ZDF) / topology-preserving transform (TPT)** filters, popularized in music-DSP practice by Zavalishin and related VA literature, solve the feedback path implicitly rather than adding a one-sample delay. This gives accurate cutoff and resonance behavior under modulation with low CPU cost.
- **Antialiased oscillator research** (BLEP/minBLEP/polyBLEP and differentiated polynomial waveform families) shows that discontinuities, not the steady waveform, are the main aliasing source. Mars therefore uses band-limited wavetable levels with analog-style phase, pitch, and pulse-width drift instead of expensive circuit solving per oscillator sample.
- **Component variation and calibration modeling** from circuit-emulation papers is applied statistically: per-voice resistor/capacitor tolerance, thermal drift, envelope leakage, pan-law mismatch, and saturation thresholds differ slightly. This gives vintage realism without solving a full SPICE netlist.

## Chosen approach

A full nodal/SPICE transient solve for every voice would be overkill for a polyphonic instrument. Mars instead uses:

1. **Band-limited analog oscillators** for saw/pulse/triangle colors, with per-voice drift and component age.
2. **Nonlinear mixer and VCA stages** with inexpensive tanh/soft-saturation transfer functions calibrated per voice.
3. **TPT/ZDF state-variable and ladder-inspired filter paths**, with WDF-style component parameters and controlled nonlinear drive around the feedback loop.
4. **Oversampling only around nonlinear hotspots when needed**, rather than across the whole synth.
5. **Block-rate control smoothing** for knobs, envelopes, and LFOs, while audio-rate modulation remains sample-accurate where it affects pitch/filter stability.

This hybrid captures the audible behavior of analog circuitry—stable resonance, modulation depth, oscillator beating, saturation memory, and voice-to-voice tolerance—while staying appropriate for real-time polyphony on ordinary CPUs.

## Reference anchors used for the design

- A. Fettweis, wave digital filter papers on digital structures derived from analog networks.
- V. Välimäki and A. Huovilainen, virtual-analog oscillator/filter work on antialiasing and nonlinear Moog-style filter modeling.
- V. Zavalishin, *The Art of VA Filter Design*, for TPT/ZDF state-variable and ladder-family filters.
- J. Parker, V. Zavalishin, and E. Le Bivic, zero-delay feedback filter modeling discussions for musical nonlinear filters.
- J. D. Parker and related DAFx literature on efficient nonlinear virtual-analog circuit modeling.
