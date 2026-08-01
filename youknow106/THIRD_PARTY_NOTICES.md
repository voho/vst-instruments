# Third-party notices

YouKnow106's original source code is covered by the `LICENSE` file included with
each source or binary distribution. YouKnow106 builds against one separately
licensed framework, which it does not relicense. The research attribution below
does not identify bundled third-party source code: no third-party source,
netlist, ROM image, firmware, sample or recording is included in this
repository.

## Modelling references

YouKnow106's engine is an independent implementation informed by published
virtual-analog research. It does not copy or adapt any of the reference
implementations that accompany these papers, and none of them is included here.

- Vadim Zavalishin, *The Art of VA Filter Design* — topology-preserving
  transforms, the integrator prewarp `g = tan(pi fc / fs)`, and the four-pole
  ladder's `1/(4 - k)` result and `k = 4` oscillation threshold.
- Tim Stilson and Julius O. Smith, *Analyzing the Moog VCF with Considerations
  for Digital Implementation* (1996) — the ladder's root locus and the
  uncoupling of resonance from cutoff.
- Antti Huovilainen, *Non-linear digital implementation of the Moog ladder
  filter* (DAFx-04), and Stefano D'Angelo and Vesa Välimäki, *Generalized Moog
  Ladder Filter: Part II* (<https://doi.org/10.1109/TASLP.2014.2352556>) —
  nonlinear delay-free-loop solutions for a transconductor cascade. YouKnow106
  solves its own implicit system with a damped Newton step whose Jacobian is
  bidiagonal plus one corner term contributed by the resonance return.
- Vesa Välimäki, Jussi Pekonen and Juhan Nam, *Perceptually informed synthesis
  of bandlimited classical waveforms using integrated polynomial interpolation*
  (JASA, 2012) — integrated-B-spline BLEP residual tables built at 64x
  oversampling with a Blackman window, which is how YouKnow106 builds its own
  step and slope residuals at construction.
- Martin Holters and Julian Parker, *A Combined Model for a Bucket Brigade
  Device and its Input and Output Filters* (DAFx-18) — bucket-brigade delay
  modelling and the support-filter corners and line gain YouKnow106 uses.

## Reference instrument

YouKnow106 models the voice architecture of the Roland Juno-106. It is an
independent original implementation and is not affiliated with, endorsed by,
sponsored by or licensed by Roland Corporation. "Roland" and "Juno" are
trademarks of Roland Corporation, used here only to identify the instrument
whose published circuit and service documentation the model is derived from.
No Roland firmware, ROM data, sample, impulse response, preset library or
recording is included in this repository.

## JUCE 8.0.14

JUCE is Copyright (c) Raw Material Software Limited.

Repository: <https://github.com/juce-framework/JUCE/tree/8.0.14>

License: JUCE framework modules are dual-licensed under the GNU Affero General
Public License version 3 (AGPLv3) and the commercial JUCE licence. Building or
distributing YouKnow106 with JUCE therefore requires either compliance with the
AGPLv3 for the complete combined work or an appropriate commercial JUCE licence.
Review the JUCE 8 licence terms before distribution:
<https://github.com/juce-framework/JUCE/blob/8.0.14/LICENSE.md>

JUCE includes or interfaces with additional third-party components. Their
copyright notices and licence terms are listed in JUCE's own `LICENSE.md` and in
the JUCE source tree. The VST3 SDK portions used through JUCE are identified
there as MIT-licensed; Apple's Audio Unit frameworks are supplied by the macOS
SDK and remain subject to Apple's terms.
