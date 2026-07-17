# OptiLab Core Winamp DSP

`dsp_optilab_core.dll` is a self-contained 32-bit classic Winamp DSP/Effect
plug-in from LanesAudio. It is a native Windows port of the OptiLab Core JSFX
processing design, built so Winamp and compatible broadcast hosts can use the
same Core sound without REAPER.

The Winamp plug-in keeps the same simple Core idea: choose a mode, set the input
drive, and optionally raise Auto-adapt for more automatic leveling, tone
balancing, and protection.

## Controls and accessibility

The settings window uses standard Windows dialog controls only. It supports
screen-reader control names, Tab and Shift+Tab navigation, Alt access keys,
native focus indication, Enter for OK, and Escape for Cancel. It has no custom
drawing, unlabeled graphics, or mouse-only controls.

- **Mode:** Podcast Leveler, Stream polish, or Smooth Limiter
- **Input drive:** -12.0 through +18.0 dB
- **Auto-adapt:** 0 through 100 percent
- **Show visual meters:** optional live meter bars and text readouts for input
  peak, output peak, and recent full-scale sample activity. This is off by
  default.
- **Restore Defaults:** Podcast Leveler, +3.5 dB, and 0 percent

When switching modes, the Winamp DSP uses host-calibrated starting Input values.
Stream polish starts at `+1.0 dB`, which is gentler than the REAPER JSFX default
and better matched to Winamp-compatible integer PCM hosts.

Settings are stored separately for each host application under:

```text
HKEY_CURRENT_USER\Software\LanesAudio\OptiLab Core\Winamp DSP\<host.exe>
```

No administrator access is needed to save settings.

## Installation

1. Close the audio host.
2. Copy `dsp_optilab_core.dll` into the host's Winamp DSP plug-in folder.
3. Restart the host.
4. Open its DSP/Effect configuration and select **OptiLab Core 1.1.2**.
5. Click **Configure** to choose the mode, Input drive, and Auto-adapt amount.

Classic Winamp normally uses:

```text
C:\Program Files (x86)\Winamp\Plugins
```

Copying into `Program Files (x86)` may require administrator approval. RadioBOSS,
StationPlaylist Studio, and other compatible applications may use a folder
selected in their own DSP configuration. Consult the host documentation rather
than copying the DLL into several unknown directories.

## Audio formats

The adapter supports mono and stereo integer PCM at 8, 16, 24, or 32 bits. The
16-bit path is specialized for the common 44.1 and 48 kHz broadcast cases.
Unsupported formats and channel layouts pass through unchanged.

For best results, start with Auto-adapt at 0 percent, adjust Input drive until
the sound is lively but not strained, then raise Auto-adapt if you want more
automatic balancing.

## Output ceiling and sample-rate conversion

OptiLab Core applies its final sample ceiling before returning audio to the
host. If the host converts sample rates after the DSP plug-in, the conversion
filter can create new sample peaks above that ceiling. For a controlled ceiling
test, keep host processing and output at the same sample rate. If conversion is
required for delivery, measure the converted output and perform any required
true-peak limiting after conversion.

## Troubleshooting

If the plug-in does not appear:

1. Confirm that the host supports classic 32-bit Winamp DSP plug-ins.
2. Confirm that the DLL is in the exact folder configured by that host.
3. Fully close and restart the host after copying the DLL.
4. Check the DLL Properties > Details page for Company `LanesAudio` and Product
   version `1.1.2`.
5. Remove older copies from other plug-in folders to avoid loading the wrong DLL.

The Release DLL statically links the Microsoft C/C++ runtime, so a separate
Visual C++ Redistributable should not be required.

## Removal

Close the host and delete `dsp_optilab_core.dll` from its plug-in folder. Optional
per-host settings can be removed from the registry path shown above.
