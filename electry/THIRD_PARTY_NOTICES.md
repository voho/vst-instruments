# Third-party notices

Electry's original source code is covered by the `LICENSE` file included with
each source or binary distribution. Electry builds against one separately
licensed framework, which is not relicensed by Electry. The research
attribution below does not identify bundled third-party source code.

## Physical-modeling research attribution

Electry's string, player-interaction, and pickup structures follow published
research. No source code from these works is included; the engine is an
independent implementation, and each model's precise claim boundary is
documented in `Docs/physical-modeling-research.md`:

- M. Karjalainen, V. Välimäki, and T. Tolonen, *Plucked-String Models: From
  the Karplus-Strong Algorithm to Digital Waveguides and Beyond*, Computer
  Music Journal 22(3), 1998.
- T. Tolonen, V. Välimäki, and M. Karjalainen, *Modeling of tension
  modulation nonlinearity in plucked strings*, IEEE Transactions on Speech
  and Audio Processing 8(3), 2000.
- J. Rauhala and V. Välimäki, *Dispersion modeling in waveguide piano
  synthesis using tunable allpass filters*, DAFx 2006; J. S. Abel and
  J. O. Smith, *Robust Design of Very High-Order Allpass Dispersion
  Filters*, DAFx 2006.
- R. C. D. Paiva, J. Pakarinen, and V. Välimäki, *Acoustics and Modeling of
  Pickups*, Journal of the Audio Engineering Society 60(10), 2012.
- G. Evangelista and F. Eckerholm, *Player-Instrument Interaction Models for
  Digital Waveguide Synthesis of Guitar: Touch and Collisions*, IEEE
  Transactions on Audio, Speech, and Language Processing 18(4), 2010;
  F. G. Germain and G. Evangelista, *Synthesis of guitar by digital
  waveguides: Modeling the plectrum in the physical interaction of the
  player with the instrument*, IEEE WASPAA 2009.
- S. Bilbao and A. Torin, *Numerical Modeling and Sound Synthesis for
  Articulated String/Fretboard Interactions*, Journal of the Audio
  Engineering Society 63(5), 2015.
- H. Fleischer (with T. Zwicker), dead-spot studies of electric guitars and
  basses relating neck conductance to string decay.
- J. Pakarinen, T. Puputti, and V. Välimäki, *Virtual Slide Guitar*,
  Computer Music Journal 32(3), 2008.
- N. H. Fletcher and T. D. Rossing, *The Physics of Musical Instruments*,
  Springer (stiff-string inharmonicity).

"Gibson", "Les Paul", "Fender", and "Telecaster" are trademarks of their
respective owners. Electry uses these names solely to identify the reference
styles between which its modeling axes interpolate; no affiliation,
endorsement, or capture-accurate reproduction is claimed.

## JUCE 8.0.14

JUCE is Copyright (c) Raw Material Software Limited.

Repository: <https://github.com/juce-framework/JUCE/tree/8.0.14>

License: JUCE framework modules are dual-licensed under the GNU Affero General
Public License version 3 (AGPLv3) and the commercial JUCE licence. Building or
distributing Electry with JUCE therefore requires either compliance with the
AGPLv3 for the complete combined work or an appropriate commercial JUCE
licence. Review the JUCE 8 licence terms before distribution:
<https://github.com/juce-framework/JUCE/blob/8.0.14/LICENSE.md>

JUCE includes or interfaces with additional third-party components. Their
copyright notices and licence terms are listed in JUCE's own `LICENSE.md` and
in the JUCE source tree. The VST3 SDK portions used through JUCE are identified
there as MIT-licensed; Apple's Audio Unit frameworks are supplied by the macOS
SDK and remain subject to Apple's terms.

No neural-network weights, guitar recordings, sample libraries, or impulse
responses are included in this repository or its build products.
