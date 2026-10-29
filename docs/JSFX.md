# JSFX companion

`Effects/optilab_core.jsfx` restores JSFX as a CP13 release target. It implements
the same three controls and processing trajectory as the native Core. Native
Core, VST3, CLAP, foobar2000 and Winamp DSP sources were not changed for this port.
The Winamp integer-PCM delivery safeguard remains specific to that wrapper.

Copy the file to the `Effects` folder in the REAPER resource directory. In
REAPER, use **Options > Show REAPER resource path in explorer/finder** to find
that directory. The effect opens on Stream polish, Input Drive 0 dB and
Auto-Adapt 0%.

The JSFX Mode values retain their existing session meaning: 0 is Podcast
Leveler, 1 is Stream polish, and 2 is Smooth Limiter. Native formats list Stream
first. Selecting a mode restores that mode's Input Drive default; loading saved
JSFX state preserves its saved drive. The companion keeps the same public controls and trajectory as native Core;
its coordinator scratch state is an implementation detail rather than a published control map.

## Reproducible validation

Run `VALIDATE_JSFX.bat` for the normal gate or add `-Thorough` for the extended
matrix. The runtime is pinned to ysfx revision
`8077347ccf4115567aed81400281dca57acbb0cc`; it is a build/test dependency and is
not included in the plugin payload. Tests use generated signals and native Core
output as their reference, without private recordings or named parameter maps.

The standard gate checks 324 coordinator settings, 705 audio/state cases, and
27 legacy slider-only session recalls into an already-running different mode.
The thorough gate checks 1,275 audio cases: 486 rate/drive comparisons, 120
longer mixed, bass-heavy, bright and silent signals, 63 live-update/state cases (42 state-transition cases plus 21 direct UI-write cases),
and 606 bidirectional slider-sweep cases. The same 27 legacy-session cases also run, for 1,302 audio comparisons
in total. Rates span 8 kHz to 768 kHz. Both absolute sample error and normalized
RMS error must remain at or below 0.000001; reported latency must match exactly.
These are numerical regression limits, not a claim of bit identity for every
possible input or host.

The original CP13 restoration measurements passed all 405 rate/drive and 120 longer
cases, with a largest sample difference of 0.00000005960464477539063. The
42 live-update/state cases and 27 legacy-session cases also passed. Reports identify the
exact tested source and runtime.

## Release acceptance

The maintainer confirmed REAPER host, accessibility and listening acceptance
for version 1.5.0 on October 29, 2026. This acceptance is separate from the
headless DSP regression results above. Future releases still require the
checks in [the release procedure](../RELEASING.md).
