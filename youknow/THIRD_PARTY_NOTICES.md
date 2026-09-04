# Third-party notices

YouKnow's original source code is covered by the `LICENSE` file included with
each source or binary distribution. YouKnow builds against one separately
licensed framework, which it does not relicense. No third-party source code,
netlist, ROM image, firmware, sample or recording is included in this
repository. The historical functional tone-memory data described below is
included separately from the original source code.

## Modelling references

YouKnow's engine is an independent implementation informed by published
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
  nonlinear delay-free-loop solutions for a transconductor cascade.
- Vesa Välimäki, Jussi Pekonen and Juhan Nam, *Perceptually informed synthesis
  of bandlimited classical waveforms using integrated polynomial interpolation*
  (JASA, 2012) — integrated-B-spline BLEP residual tables built at 64x
  oversampling with a Blackman window, which is how YouKnow builds its own
  step and slope residuals at construction.
- Martin Holters and Julian Parker, *A Combined Model for a Bucket Brigade
  Device and its Input and Output Filters* (DAFx-18) — bucket-brigade delay
  modelling and the support-filter corners and line gain YouKnow uses.

## Reference instrument

YouKnow models the voice architecture of the Roland Juno-106. It is an
independent original implementation and is not affiliated with, endorsed by,
sponsored by or licensed by Roland Corporation. "Roland" and "Juno" are
trademarks of Roland Corporation, used here only to identify the instrument
whose published circuit and service documentation the model is derived from.
No Roland firmware, ROM image, sample, impulse response, captured audio or
Roland Cloud product content is included in this repository.

## Factory tone-memory data

The repository includes the original 128 factory tone states as 2,304 bytes of
functional parameter data: 128 records of sixteen 7-bit control bytes plus two
packed switch bytes. In physical memory order A11..A88/B11..B88, their SHA-256
is `394ae874da33aa63fa4833932fbf415546d2ad66b1b6b9a36315601799eeec21`.

The corpus was mechanically decoded and matched byte for byte across these
public archives:

- [Juno-106 Connection](http://www.hinzen.de/midi/juno-106/): the Bank A and
  Bank B tape WAV files and `Factory_Patches.pat`.
- [Jarvik7 Juno-106 Librarian](https://www.jarvik7.net/juno-106/): the
  position-preserving factory `.106` library.
- [KR-106](https://github.com/kayrockscreenprinting/ultramaster_kr106/tree/bc15caee5843ab238a25d0969e68d57db2b1615f/tools/preset-gen):
  its factory PAT and decoded JSON transcription.

The displayed descriptive names are archival metadata; patch-name text does not
exist in the hardware memory records. The project's MIT license covers
YouKnow's original code and assets and does not assert ownership of Roland
trademarks or the historical tone states. The cited archives do not provide an
explicit MIT grant for this corpus, so redistributors should make their own
rights assessment. No data was taken from the separately distributed Roland
Cloud Original 128 product.

## JUCE 8.0.14

JUCE is Copyright (c) Raw Material Software Limited.

Repository: <https://github.com/juce-framework/JUCE/tree/8.0.14>

License: JUCE framework modules are dual-licensed under the GNU Affero General
Public License version 3 (AGPLv3) and the commercial JUCE licence. Building or
distributing YouKnow with JUCE therefore requires either compliance with the
AGPLv3 for the complete combined work or an appropriate commercial JUCE licence.
Review the JUCE 8 licence terms before distribution:
<https://github.com/juce-framework/JUCE/blob/8.0.14/LICENSE.md>

JUCE includes or interfaces with additional third-party components. Their
copyright notices and licence terms are listed in JUCE's own `LICENSE.md` and in
the JUCE source tree. The VST3 SDK portions used through JUCE are identified
there as MIT-licensed; Apple's Audio Unit frameworks are supplied by the macOS
SDK and remain subject to Apple's terms.

## Generated UI assets

### Faceplate material tile

`used-charcoal-plastic.png` is a 1024 px, edge-to-edge material tile generated
for this instrument's faceplate. It depicts maintained but well-used charcoal
ABS: fine mould grain, subtle satin wear, cleaning swirls, hairline scuffs, and
small shallow nicks. The editor embeds it as binary data and composites it at
low contrast, so no loose file is required at runtime.

Generation mode: OpenAI built-in image generation, new image (not an edit).
SHA-256: `cfcdd00f5d885eff061ebee8e3b120dd619a704849e128999bf08ac5feb6b920`.

Prompt:

> Use case: ui-mockup. Asset type: seamless texture tile for a virtual
> synthesizer faceplate. Primary request: create a square, edge-to-edge
> tileable surface texture of dark charcoal injection-moulded ABS plastic from
> a well-used 1980s electronic musical instrument. Style/medium: photorealistic
> material scan, orthographic, no perspective. Lighting/mood: extremely even
> diffuse overhead lighting; almost no directional shadow; low-contrast surface
> information suitable for subtle UI compositing. Color palette: neutral
> graphite and charcoal grayscale only. Materials/textures: fine mould grain,
> slight uneven satin sheen, sparse hairline scuffs, softly polished high-touch
> patches, a few tiny shallow nicks and dust-cleaning swirls; convincingly used
> but maintained, never dirty or damaged. Composition/framing: uniform material
> covering the entire square; seamless/tileable on all four edges; no border
> and no focal point. Constraints: no controls, knobs, sliders, screws, panels,
> labels, text, logos, grooves, seams, holes, objects, highlights that imply a
> single light direction, deep scratches, rust, grime, fingerprints, stains, or
> watermark; texture must remain readable when tiled at low opacity behind
> legible UI text.

### Standalone app icon

`youknow-icon.png` is the 1024 px alpha-bearing source used by JUCE to
generate the standalone application's platform icons. It is an original
six-fader mark rather than a reproduction of the reference instrument's panel.

Generation mode: OpenAI built-in image generation, new image, generated
2026-08-17 and resampled to 1024 px for the project. SHA-256:
`c1777c084a19f48135789657157d23c535399461a31448874093f24dda282068`.

Prompt:

> Use case: logo-brand. Asset type: 1024x1024 macOS standalone synthesizer app
> icon. The YouKnow editor screenshot is a visual-language reference only;
> do not reproduce its panel or controls literally. Create one restrained,
> premium icon for an original circuit-modelled six-voice analog-style
> synthesizer: a dark charcoal rounded-square instrument tile with six slim
> vertical fader/light channels, a thin warm-red rail near the top and a thin
> cyan rail near the bottom. Use a crisp high-end product-icon treatment with
> subtle molded ABS grain and restrained physical depth, readable at 16-32 px.
> Use charcoal, near-black, muted off-white, restrained warm red and cyan. Keep
> generous inset and a bold silhouette, with genuine transparency outside the
> rounded square. No text, numbers, logos, keyboard keys, brand marks,
> watermark, or imitation of Roland/Juno trade dress.
