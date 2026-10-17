# Airwindows distortion DSP

Imported from Lodo commit `9173fb9` (DSP introduced on 2026-09-30).
Pinned upstream: `d22a25b7f7c0c8c05e9f3ae480e7f1da9769f49d` in https://github.com/airwindows/airwindows.
MIT license: see `LICENSE`. Only the 13 adapted distortion kernels are included here; retained upstream references and import tools mentioned below live in Lodo.

## Distortion kernels (2026-09-30)

The same pinned MIT revision supplies Drive, Fracture, ToTape9,
IronOxideClassic2, Density3, Spiral2, Tube2, Mackity, Coils2, FireAmp,
DrumSlam, BitGlitter and DeBez. Each unmodified header, plugin shell and complete
processing file is retained in `upstream/<name>/` with its SHA-256 in
`upstream.json`. No VST SDK is copied or compiled.

`tools/airwindows/import_distortion_kernels.py` reproduces the SDK-free float
kernels from those pinned originals. Native callback setup is separated into
`beginBlock(frames)` and the original loop body into `processFrame(left,right)`.
Scalar voice FX retain their original position in the chain while native
coefficients/control setup runs once per rendering/event span. DeBez retains its
remaining-frames interpolation; native precision, stereo histories, denormal
handling and float dither are retained. Constructor random seeds become the same
fixed nonzero seeds used by the source-reference runner. All native histories
are trivially copyable; initialized templates are built before audio starts.

Lodo uses one Amount/Output/Mix presentation for every distortion mode.
`Engine/Fx/DistortionAmountProfiles.cpp` coordinates native controls and ordinary
host input gain/wet compensation. Output scales only the wet signal before the
final dry/wet blend. Native output controls remain fixed where they affect
internal voicing. The native sample-processing algorithms remain unchanged
except for the Core transformer-residual strength extension documented below.
There are no old profile mappings or combined Drive/SR/BR playback stages.
Only current named controls are read by the distortion settings loader.

Tape uses ToTape9 with native Flutter depth and speed disabled. Host drive and
square-root wet compensation focus Amount on saturation and tonal wear.
The distortion bank links the native clipping-noise/dither seeds between
channels using the adapter's `linkStereoRandomness()` initialization method,
preventing hot mono input from acquiring a stereo difference. The processor's
default independent seeds remain available to future performance FX adapters.
Original sample processing, pinned source files, hashes and MIT notices remain
intact. The source-reference runner uses the same linked seeds for this insert.

Crush keeps DeBez at full DeRez and uses its bit control for 16-to-3.52-bit
reduction, retaining native Bezier smoothing. Native wet controls remain fully
wet; Lodo supplies the independent wet Output and final Mix.

The driven upper range of Heat, Valve, Core, Punch, Tape, Twist and Thick starts
above 25% Amount. Host drive/wet trim extends saturation and restores useful
level; Tape Bias returns to neutral at full Amount to avoid softening away the
driven waveform through overbias slew limiting. Flutter stays disabled.

Core deviation: `Coils2::setSaturationStrength()` scales only the existing
`(sin(diffSample)-sin(hysteresis))*distScaling` transformer residual, from 1x to
12x. The clean bandpass, filters, hysteresis histories, native controls and
dither are retained. Default/unity strength is upstream behavior; reset restores
unity. The Lodo Amount profile adds strength above 25% and limits host input
drive to 24x at full Amount. The importer reproduces this extension, and the
independent callback generator applies the same documented residual multiplier.
Unmodified retained upstream sources, hashes and MIT notices are unchanged.

Mackity's fixed 19.16 kHz lowpass is capped at .49 of sample rate to keep it stable
at 32 kHz. It is unchanged at 44.1 kHz and above. The common bank resets a
prepared history on non-finite output; FireAmp additionally detects runaway
output above 16 before it reaches the mixer. Abrupt endpoint automation with
hot input can otherwise poison FireAmp's native high-Q filter histories.
Recovery copies a cold prepared history and preserves the dry fraction for that frame; no
allocation, constructor, table rebuild or new distortion algorithm is involved.
Ordinary native output remains unchanged, including legitimate above-unity
levels from the other kernels.

`generate_distortion_references.py` independently compiles the retained original
float callbacks with the documented Core multiplier and the same profiles/fixed
seeds. Fixtures cover all
13 algorithms at 44.1/48/96 kHz, defaults, endpoints, parameter changes, and
1/127/128/511-frame spans. `run_distortion_probe.py` compiles the actual production
bank with ASan/UBSan by default, or optimized with `--benchmark`.

## Lodo distortion display names

The short product labels below retain the source attribution here and in the
vendored files; MIT notices, pinned originals and checksums remain intact.

| Lodo label | Upstream algorithm |
| --- | --- |
| Heat | Drive |
| Shred | Fracture |
| Tape | ToTape9 |
| Oxide | IronOxideClassic2 |
| Thick | Density3 |
| Twist | Spiral2 |
| Valve | Tube2 |
| Crunch | Mackity |
| Core | Coils2 |
| Stack | FireAmp |
| Punch | DrumSlam |
| Grit | BitGlitter |
| Crush | DeBez |
