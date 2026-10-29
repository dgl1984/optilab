# OptiLab Core 1.5.0

Version 1.5.0 rebuilds Core around the current engine and shared delivery limiter. It adds VST3, updates the classic playback wrappers, and preserves the three-control workflow.

Version 1.5 is a substantial internal rebuild while keeping the same three controls: **Mode**, **Input Drive**, and **Auto-Adapt**.

## Stream is now first

Fresh instances now open on **Stream polish**, and Stream is the first item in the native Mode list. JSFX keeps its legacy order of Podcast, Stream, and Smooth for saved-session compatibility. That matches how Core is actually used: Stream is the broadest general-purpose voicing and the mode most people tend to reach for first.

Use Stream when you want a bigger, more controlled sound across mixed content such as music, games, speech, streams and radio. At 0% Auto-Adapt, Stream now uses the released Core's starting settings through the current engine and shared delivery limiter. It blends toward the newer middle voicing, then develops the more intense upper range; this is a progression between voicings, not a calibrated loudness ramp. The upper range now reduces the drive into Density, lets the multiband compressor and limiter carry more of the control, restores level after Density, and then adds the final late-stage pressure. Presence and Brilliance withdraw to neutral by the 90% handoff so the loudest settings do not stack extra high-frequency sweetening on top of heavier Final control.

## Podcast is deliberately different

Podcast Leveler is no longer best described as a gentler Stream. It is a flatter, more consistent voicing designed around speech and the kind of control associated with modern talk stations. Different voices can take on more similar sonic weight, short peaks and plosives are restrained, and high Auto-Adapt is allowed to flatten transients when that helps intelligibility and consistency. Stereo Width stays at 100% throughout Podcast's Auto-Adapt range, so stronger adaptation does not widen the image.

It is still worth trying Podcast on music, games and other mixed material. If its flatter presentation is not what you want, use Stream. The modes are supposed to sound different.

Podcast has enough level-recovery authority to pull badly offset material back toward the rest of a program, but that is not a promise that it can erase arbitrarily large source-level differences without consequences. The upper half also changes parts of its processing strategy rather than merely turning the same settings up, so a sweep through the range can produce a few audible transitions.

## Smooth Limiter has a real destination

Smooth Limiter develops toward stronger event-limiting behavior as Auto-Adapt rises. The balance of density, limiting, and smoothing changes while the mode keeps its basic timing character.

The mode intentionally expects the user to work **Input Drive**. If the limiter is barely reacting, turn the input up into it and listen. Auto-Adapt changes how the limiter handles that pressure; Input Drive decides how much pressure you give it.

Like anything that starts off smooth, this limiter can stay smooth for a surprisingly long time—but if you keep poking at it and driving it beyond what it can tolerate, eventually **it gets an edge**.

## Shared final limiter

Podcast, Stream and Smooth now share the same final delivery-limiter implementation and ceiling policy. The modes still sound different because their identity is established before that final protection stage.

The new limiter is better at holding dense bass, brass and other periodic material together without reopening too quickly between waveform cycles. It also keeps fast transient recovery where that is useful. The release target remains approximately **-0.1 dBTP or lower** on the stress material used for release review.

The research direction was influenced by the x42 Digital Peak Limiter by Fons Adriaensen and Robin Gareus, discovered through chmaha's REAPER JSFX port. Core's implementation is independent and contains no x42 source code; the influence is credited because the x42 design prompted useful research into sustained-event behavior.

## Final audit polish

This release also fixes several host-integration and state-handling problems found during the rebuild. Input Drive now behaves consistently across the supported wrappers, fresh instances report the intended defaults, mode changes report the correct latency, and unsupported sample rates pass through safely in the classic playback wrappers.

There are also CPU and robustness improvements in the native and JSFX paths. These do not intentionally change the voicing.

## Under the hood

Version 1.5 is a substantial internal rebuild around the current Core engine while keeping the visible product deliberately small: three controls and three distinct modes. Reset behavior, state handling, realtime parameter transport, latency reporting, and wrapper behavior received another full regression pass as part of that work.

The public source contains what is required to build and maintain Core. Release review also uses additional maintainer-only listening material and regression assets that are not distributed with the public source package.

## Formats

The 1.5 native release targets:

- 64-bit Windows VST3
- 64-bit Windows CLAP
- 32-bit Winamp-compatible DSP
- 32/64-bit foobar2000 component package

The JSFX companion in `Effects/optilab_core.jsfx` is synchronized to the current Stream coordinator and endpoint states, with generated-signal processing, coordinator, latency, automation and state regression coverage against the native reference. The Windows build validates it before packaging. REAPER host, accessibility and listening acceptance are complete, as confirmed by the maintainer; see [JSFX notes](docs/JSFX.md).



### Stream redesign

Stream polish has been rebuilt around the newer OptiLab processing architecture and intentionally has a new sound. The upper end now extends into much more aggressive broadcast-style loudness and transient flattening. Users who specifically prefer the previous Core processing character can keep the older Core. The two versions are intentionally not identical.

### CLAP migration

See the [CLAP upgrade note](CHANGELOG.md#clap-upgrade-note) before replacing a pre-1.5 plugin.

## Native settings sliders

Foobar2000 and Winamp now use sliders for Input Drive and Auto-Adapt, while Mode remains a combo box. Values, ranges, mode defaults and saved settings retain their meaning. The JSFX history buffer now wraps consistently with native Core when an Auto-Adapt change shortens its window.

## foobar2000 packaging

The component includes the correct 32-bit and 64-bit DLLs. Packaging now verifies each DLL architecture before creating the component. Thanks to Nick Giannak III for the correction.
