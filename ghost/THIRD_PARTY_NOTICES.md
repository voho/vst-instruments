# Third-party notices

Ghost's original source code is covered by the `LICENSE` file included with
each source or binary distribution. Ghost builds against one separately
licensed framework, which it does not relicense. No third-party source code,
schematic reproduction, firmware, sample or recording is included in this
repository.

## Modelling references

Ghost's engine is an independent implementation informed by published
documentation of the modelled 1983 instrument and by published virtual-analog
research. It copies no reference implementation, and none is included here.

- The modelled instrument's factory owner's manual and service-manual
  schematics (Crumar s.p.a., 1983) — consulted as documentation of control
  calibration and circuit topology; no drawing, netlist or text from them is
  reproduced in this repository. Ghost is not affiliated with, endorsed by,
  or licensed by Crumar or its successors.
- Curtis Electromusic CEM3340 and CEM3350 datasheets — the voltage-control
  laws and configuration practice of the modelled oscillator and filter ICs.
- US Patent 3,943,456 (D. A. Luce, Moog Music, 1976) — the variable-rate
  integrator that is the modelled instrument's auxiliary
  envelope/LFO ("Shaper") core.
- Vadim Zavalishin, *The Art of VA Filter Design* — topology-preserving
  transforms and the state-variable filter realisation with the integrator
  prewarp `g = tan(pi fc / fs)`.
- Vesa Välimäki and Antti Huovilainen, *Antialiasing Oscillators in
  Subtractive Synthesis* (IEEE Signal Processing Magazine, 2007) — the
  polynomial bandlimited step (PolyBLEP) correction family used by the
  oscillators.

## JUCE 8.0.14

JUCE is Copyright (c) Raw Material Software Limited.

Repository: <https://github.com/juce-framework/JUCE/tree/8.0.14>

License: JUCE framework modules are dual-licensed under the GNU Affero
General Public License version 3 (AGPLv3) and the commercial JUCE licence.
Building or distributing Ghost with JUCE therefore requires either compliance
with the AGPLv3 for the complete combined work or an appropriate commercial
JUCE licence. Review the JUCE 8 licence terms before distribution:
<https://github.com/juce-framework/JUCE/blob/8.0.14/LICENSE.md>

A copy of JUCE's licensing README is included at
[`ThirdParty/JUCE-LICENSE.md`](ThirdParty/JUCE-LICENSE.md). JUCE itself is
downloaded at build time (pinned to its 8.0.14 release commit) and is not
vendored in this repository.
