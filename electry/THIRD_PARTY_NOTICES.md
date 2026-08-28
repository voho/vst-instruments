# Third-party notices

Electry's original source code is covered by the `LICENSE` file included with
each source or binary distribution. Electry builds against one separately
licensed framework, which is not relicensed by Electry. The research
attribution below does not identify bundled third-party source code. The CC0
cabinet impulse data included by Electry is identified separately below.

## Physical-modeling research attribution

Electry's string, player-interaction, and pickup structures follow published
research. No source code from these works is included; the engine is an
independent implementation, and each model's precise claim boundary is set out
under "References and claim boundaries" in `README.md`:

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

## Jester Dyne Brutal IR Pack #14

Creator/publisher: Jester Dyne Productions

Source pack: <https://www.jester-dyne-productions.com/brutal-ir-pack/>

License: the source pack's handbook dedicates the impulse responses under
[CC0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/).

`Source/DSP/ModernCabinetIR.h` contains the exact first 1,024 causal samples
of the 48 kHz, 24-bit mono `14_Cathode_Ray_Fleshburn.wav` SM57/Vintage 30
capture. The original ZIP member is
`Jesters_Brutal_Pack_1.0/Impulses/48kHz/14_Cathode_Ray_Fleshburn.wav`.
Only those 1,024 sample values are included; the full ZIP, complete WAV, and
handbook PDF are not included. The prefix hash below is calculated over the
source values' original signed-24-bit little-endian PCM encoding.

- Source ZIP SHA-256:
  `299dc053f01ebd1e980459adc48f9c6b8a8c7af91917b4f946512eefdbb311ea`
- Complete source WAV SHA-256:
  `420280d44a6cb969d0599aa88f7bc733e13d39cdd051acf8b0eda1d82286ba5f`
- Source PCM bytes corresponding to the included 1,024-sample prefix SHA-256:
  `a9b7e39f38c38d820ff9b758577293b6d4cb5de03bb89090ef0763a7eb357d45`
- Source handbook PDF SHA-256:
  `265e887fc747a154916bf56408e9c4a371c9d9036aaf1b22997ad4d161cd079e`

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

No neural-network weights, guitar recordings, or sample libraries are included
in this repository or its build products.
