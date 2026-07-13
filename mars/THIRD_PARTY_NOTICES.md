# Third-party notices

Mars's original source code is covered by the `LICENSE` file included with each
source or binary distribution. Mars builds against one separately licensed
framework, which is not relicensed by Mars. The research attribution below does
not identify bundled third-party source code.

## Generalized nonlinear Moog ladder research

Mars's four-stage `Ladder` is informed by Stefano D'Angelo and Vesa Valimaki's
paper *Generalized Moog Ladder Filter: Part II - Explicit Nonlinear Model
through a Novel Delay-Free Loop Implementation Method*:

<https://doi.org/10.1109/TASLP.2014.2352556>

Mars independently solves the original implicit bilinear transistor equations
with a bounded, residual-decreasing damped Newton method. It does not copy or
adapt the paper's separately published `moog_ladder_nonlinear.m` reference
implementation, and that implementation is not included in this repository.

## JUCE 8.0.14

JUCE is Copyright (c) Raw Material Software Limited.

Repository: <https://github.com/juce-framework/JUCE/tree/8.0.14>

License: JUCE framework modules are dual-licensed under the GNU Affero General
Public License version 3 (AGPLv3) and the commercial JUCE licence. Building or
distributing Mars with JUCE therefore requires either compliance with the
AGPLv3 for the complete combined work or an appropriate commercial JUCE
licence. Review the JUCE 8 licence terms before distribution:
<https://github.com/juce-framework/JUCE/blob/8.0.14/LICENSE.md>

JUCE includes or interfaces with additional third-party components. Their
copyright notices and licence terms are listed in JUCE's own `LICENSE.md` and
in the JUCE source tree. The VST3 SDK portions used through JUCE are identified
there as MIT-licensed; Apple's Audio Unit frameworks are supplied by the macOS
SDK and remain subject to Apple's terms.

No third-party neural-network model, polysynth recording, or sample library is
included in this repository.
