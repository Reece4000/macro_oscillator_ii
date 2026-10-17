---
name: Macro OSC II
description: Macro OSC inverted. The same flat, type-led instrument and Braids colours, light where Macro OSC is dark and dark where it is light.
colors:
  background: "#cfd1d6"
  header: "#c2c4ca"
  ink: "#111216"
  muted: "#4f535b"
  faint: "#b0b3ba"
  hairline: "#a4a7af"
  dot: "#bcbfc5"
  turquoise: "#34c6cc"
  turquoise-line: "#1f9fa5"
  magenta: "#e8437a"
  display: "#e8fa42"
  chip: "#121418"
  strip: "#15171b"
  strip-ink: "#eceef2"
  strip-muted: "#8d929d"
  strip-track: "#2c2f36"
  strip-graphic: "#c6c8ce"
typography:
  menu: { fontFamily: Geist Mono, fontWeight: 400, fontSize: "12pt" }
  section: { fontFamily: Geist, fontWeight: 500, fontSize: "22pt" }
  label: { fontFamily: Geist, fontWeight: 500, fontSize: "14pt" }
  value: { fontFamily: Geist Mono, fontWeight: 400, fontSize: "12pt" }
  model-readout: { fontFamily: DSEG14 Modern, fontSize: "28pt" }
layout:
  size: "960 × 640, scalable 75–150% at a fixed aspect ratio"
  corners: "square everywhere"
  margin: "28"
---

# Design System: Macro OSC II

## North star

Macro OSC II is Macro OSC inverted. Layout, type, controls and behaviour are shared with Macro OSC, so a player of one can play the other at once; the colour scheme is turned inside out so the two never look alike. Macro OSC is a dark instrument with a silver band; Macro OSC II is a silver instrument with a dark strip. Both keep the Braids hardware's colours: turquoise and magenta caps, and a yellow-green segment readout in a dark window.

## Colour

- A pale silver canvas; ink for labels and ticks, muted grey for section titles and values. The header is a shade darker than the canvas, as Macro OSC's is a shade lighter.
- Turquoise marks harmonics, morph, the filter and modulation; magenta marks timbre and the drive section. Fills use the cap colours; thin lines (waveform, filter curve, MSEG curve, handles) use a deeper turquoise so they read on silver.
- The dark strip (pitch and envelope) inverts the scheme again: light labels, and drawings, fills and pitch readouts in silver.
- The model readout stays a dark window with the yellow-green display, as on the module. Yellow-green appears nowhere else.
- The current model in the map is lit in ink, as the old silver band's fills were.
- No glows on silver: LEDs are flat fills. Only the readout glows, inside its window.
- Every shape has square corners.

## Type

All labels are lowercase. Section titles (osc, drive, filter, mod) are large and muted; parameter labels are ink; values are set in Geist Mono so they don't jitter. The model readout is DSEG14 with the unlit segments showing behind.

The mark is a plait of three strands, magenta, turquoise and ink, where Macro OSC's is a braid of two rings. At every crossing the strand coming in from the outside passes over the middle one, as in a real plait; a gap cut into the strand beneath shows the weave. On dark grounds (the app icon) the third strand is silver.

## Layout

As Macro OSC: a header strip (settings menu, mark and preset name on the left; volume, voice count and voice mode on the right); oscillator on the left (readout, model map, waveform, four macros) beside drive and filter on the right; the dark pitch/envelope strip across the full width; modulation at the bottom.

The view is designed at 960 × 640 and scaled as a whole by the editor, so every coordinate in `EditorView::resized` is a design unit.

## Components

Components not listed here behave as in Macro OSC's design system.

- **Model readout:** the model number (01–24) and name in DSEG14, in a dark window. Click for the three-column model menu; the arrows step through all 24; the grid button opens the menu.
- **Model order:** grouped by kind of sound, not by Plaits' firmware banks: synth (10), FM (5), paraphonic (3), physical (3), percussive (3). `kPlaitsModels` maps each entry to its Plaits engine.
- **Model map:** 24 cells in those five families, every family labelled; the family of the current or hovered model reads in ink.
- **Oscillator macros:** harmonics, timbre and morph (0–127, as Macro OSC's macros), then aux, which blends from the main output to the model's auxiliary output.
- **Waveform:** the selected model rendered offline at its base Harmonics, Timbre, Morph and Aux: two and a half cycles for steady models, the decay from a strike for string, modal and the drums. It does not show modulation or effects.

## Do and don't

- **Do** keep every sound-shaping parameter on one page, and keep the layout in step with Macro OSC.
- **Do** keep the hierarchy: ink for what you touch, muted for what explains it.
- **Do** be honest about previews: the waveform and drive plots are references, not live audio.
- **Don't** add bevels, gradients, drop shadows, rounded corners or focus boxes.
- **Don't** use yellow-green outside the model readout.
- **Don't** introduce knobs; parameters are sliders or direct-manipulation plots.
