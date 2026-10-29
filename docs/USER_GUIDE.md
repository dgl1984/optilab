# Using OptiLab Core

Choose the job you want Core to do, decide how hard to feed it, then turn Auto-Adapt until the result is where you want it. There is no magic percentage that is right for every recording.

This guide starts with getting Core into your host, then walks through the interface, the three controls, and each Mode. The listening advice matters more than finding somebody else's perfect setting.

1. [Getting started](#getting-started)
2. [Finding your way around](#finding-your-way-around)
3. [The three controls](#the-three-controls)
4. [Stream polish](#stream-polish)
5. [Podcast Leveler](#podcast-leveler)
6. [Smooth Limiter](#smooth-limiter)
7. [Saved settings and defaults](#saved-settings-and-defaults)
8. [Output level](#output-level)
9. [Troubleshooting](#troubleshooting)
10. [Frequently asked questions](#frequently-asked-questions)
11. [The story behind Core](#the-story-behind-core)

## Getting started

### Install the format your host uses

The native packages are for Windows. VST3 and CLAP are 64-bit; the Winamp-compatible DSP is 32-bit. The foobar2000 component includes both 32-bit and 64-bit versions. JSFX runs inside REAPER.

- **VST3:** copy the complete `OptiLab Core.vst3` bundle into your host's VST3 location, normally `C:\Program Files\Common Files\VST3`, then rescan. Keep the bundle together.
- **CLAP:** copy `OptiLab Core.clap` into your host's CLAP location, such as `%LOCALAPPDATA%\Programs\Common\CLAP`, then rescan. If you are replacing the pre-1.5 CLAP, read the [upgrade note](../CHANGELOG.md#clap-upgrade-note) first.
- **Winamp-compatible DSP:** close the host, copy `dsp_optilab_core.dll` into its DSP plugin folder, restart it, and select OptiLab Core in its DSP settings. The folder depends on the host.
- **foobar2000:** install `foo_optilab_core-1.5.0.fb2k-component` through **Preferences > Components**, restart when prompted, then add OptiLab Core under **Preferences > Playback > DSP Manager**. Do not unpack the component and copy a DLL manually.
- **JSFX:** in REAPER, choose **Options > Show REAPER resource path in explorer/finder**, copy `Effects/optilab_core.jsfx` into that directory's `Effects` folder, then find Core in the FX browser. See the [JSFX notes](JSFX.md) for saved-session differences.

### Your first listen

1. Add Core to the track, bus, or playback DSP chain you want to process. Start with material you know well, including its louder and quieter parts.
2. Start on **Stream polish**, with **Input Drive** at 0 dB and **Auto-Adapt** at 0%.
3. Raise Auto-Adapt gradually. If the sound becomes too dense, too flat, or too pushed, back it off. Adjust Input Drive when you want to change how hard the audio hits Core.
4. Compare with your host's bypass at a similar listening level. Louder can be impressive for a moment without actually sounding better.
5. Try the other Modes when their jobs fit the material. Changing Mode loads that Mode's starting Input Drive, so check the value before continuing.

In the Winamp and foobar2000 settings dialogs, press **OK** to apply the changes before judging the result.

## Finding your way around

Core has three processing controls, in this order: **Mode**, **Input Drive**, and **Auto-Adapt**. All three are available in every Mode. Changing Mode changes what the processing does; it does not open another page of controls.

When this guide talks about bass control, stereo width, compression, Density, saturation, or limiting, it is describing what Core does with your audio. Those are not separate knobs in Core.

### VST3 and CLAP

The editor presents Mode first, then Input Drive and Auto-Adapt. Tab and Shift+Tab move through them. The Mode menu contains Stream polish, Podcast Leveler, and Smooth Limiter, in that order.

On Input Drive, arrows change 0.1 dB and Page Up/Down change 1 dB. On Auto-Adapt, arrows change 1% and Page Up/Down change 10%. Home selects the maximum and End the minimum on those two sliders. On Mode, Home selects Stream polish and End selects Smooth Limiter.

Press Enter or F2 on a slider for exact value entry, then Enter to accept or Escape to cancel. Auto-Adapt exact entry takes whole percentages. Moving focus away also cancels. Plain numbers, displayed units, and comma decimals are accepted; invalid or out-of-range entries leave the previous value alone.

Your host's generic parameter interface remains available, including REAPER/OSARA. See [Keyboard and accessibility](CLAP_ACCESSIBILITY.md) for the keyboard reference and reporting host-specific problems.

### Winamp-compatible hosts

The settings dialog presents **Mode**, **Input drive (dB)**, and **Auto-adapt (%)**, followed by **Show visual meters**. Input Drive and Auto-Adapt are sliders with value readouts. Input Drive runs from -18.0 to +18.0 dB in 0.1 dB steps; Auto-Adapt runs from 0% to 100%.

Show visual meters turns on the input peak, output peak, and full-scale displays. It is off by default and does not change the sound. These are peak displays, not a loudness target to chase.

At the bottom, **Restore Defaults** selects Stream polish, 0 dB Input Drive, 0% Auto-Adapt, and meters off. **OK** applies and saves the settings; **Cancel** discards the edits. The meter checkbox previews its display change while the dialog is open; Cancel restores its previous setting too.

### foobar2000

The settings dialog presents **Mode**, **Input drive**, and **Auto-adapt**. Mode is a combo box. Input drive is a slider running from -18.0 to +18.0 dB in 0.1 dB steps, with a dB readout. Auto-adapt is a 0–100% slider with a percentage readout.

On these native sliders, arrows make a small change; Page Up/Down make a larger change (1 dB or 10%). Home selects the minimum and End the maximum. This differs from the JUCE editor's Home/End convention.

Then come **Restore Defaults**, **OK**, and **Cancel**. Restore Defaults selects Stream polish, 0 dB, and 0%. OK applies the edited setup; Cancel leaves the previous setup alone. This dialog does not have the Winamp visual-meter option.

### REAPER JSFX

REAPER exposes the same three sliders: **Mode**, **Input Drive dB**, and **Auto-Adapt percent**. The Mode slider lists Podcast Leveler, Stream polish, and Smooth Limiter in its older order, while a fresh instance still selects Stream polish.

The custom display also shows those controls and an input/output **Peak View**. Use REAPER's normal sliders or host parameter access for keyboard and screen-reader operation; the JUCE exact-entry keys above belong to VST3 and CLAP. The peak display is a visual aid, not another processing option or an independent true-peak measurement.

## The three controls

### Mode

Mode changes the job Core is trying to do.

**Stream polish** is the default. Use it when you want a bigger, more controlled sound across mixed content such as music, games, speech, streams and radio. It is the mode most people tend to reach for first, and it gives the broadest picture of what Core can do.

**Podcast Leveler** is for a flatter, more consistent sound that helps very different voices and sources feel as though they belong in the same program. Its overall approach is similar in spirit to modern talk-station processing: speech stays controlled, short peaks are restrained, and different voices can take on more similar sonic weight. It is designed around speech, but try it on music, games, mixed content, or anything else where that tighter presentation works.

**Smooth Limiter** is for already reasonably balanced material when the job is smooth loudness and peak control rather than creating as much broadcast character.

The native Mode menu lists Stream polish, Podcast Leveler, then Smooth Limiter. The REAPER JSFX slider keeps Podcast Leveler, Stream polish, then Smooth Limiter so older sessions retain their meaning; it still starts on Stream polish.

Changing Mode deliberately loads that mode's normal starting Input Drive. Stream polish and Smooth Limiter start at 0 dB. Podcast Leveler starts at +3.5 dB.

### Input Drive

Input Drive controls how hard Core is fed. Its range is -18 dB to +18 dB in every format, with 0 dB halfway along the slider. Input Drive moves in 0.1 dB steps.

If the sound becomes too dense, too flat, or too pushed, turn Input Drive down. If Core is barely doing anything, turn it up, although you may not have to if you use higher Auto-Adapt settings.

You do not need to add gain just because the control is there. If your material already arrives at a healthy, consistent level, 0 dB may be exactly right.

Input Drive and Auto-Adapt do different jobs. Input Drive changes how hard the signal hits the processor. Auto-Adapt changes how much authority the selected Mode is allowed to use.

### Auto-Adapt

Auto-Adapt changes several parts of the selected Mode together. Its range is 0% to 100%, in 1% steps. Even at 0%, Core is processing audio; use your host's bypass control when you want to compare with the original.

It is an **authority control**, not a quality control. Higher numbers do not automatically mean better sound.

At lower settings, Core stays closer to the basic character of the selected Mode. As you turn Auto-Adapt up, the processor is allowed to do more to control level, shape the sound, manage peaks, and make the overall presentation more consistent. What “more” means depends on the Mode.

In **Stream polish**, higher Auto-Adapt gradually moves from open, forgiving processing toward a bigger, denser, more assertive broadcast sound.

In **Podcast Leveler**, higher Auto-Adapt makes different voices and sources fill the available space more consistently. Transients become flatter, sudden peaks are controlled more strongly, and radically different material can begin to feel more like part of one program.

In **Smooth Limiter**, higher Auto-Adapt changes how the limiting system handles pressure, but Input Drive remains the control you use to decide how hard to hit it.

The easiest way to set Auto-Adapt is still the obvious way: turn it while you listen. There is no percentage that is automatically correct for every source.

## Stream polish

Stream polish starts with a real processing sound at 0%, then develops toward more intense broadcast-style processing as Auto-Adapt rises. Zero is a starting voicing, not a nearly dry setting.

Use it when you want a bigger, more controlled sound across mixed content such as music, games, speech, streams and radio. This is the fresh-instance default because it is the mode most people tend to reach for first and the broadest demonstration of what Core is about.

There is no high-end roll-off filter in Stream, at any Auto-Adapt setting. Bright material can still be controlled by the processing, but there is no added top-end cut.

The low end brings back the released Core's Stream starting settings, working through the current engine and shared final limiter. It blends toward the newer middle voicing through 70%, then develops the stronger upper range. That combines the useful starting character of the earlier Core with the intensity available in the newer version. It does not make the audio identical to the old release.

More intense is the useful way to think about the direction of travel. Several parts of the processor are moving together, and some processing structures switch along the way. Do not expect every tiny movement to be equally noticeable, or every higher number to be louder on every recording.

At 100%, Stream reaches the **“I’ve got something to prove”** setting. It is deliberately aggressive: transients can become very flat, quiet details are pulled forward, and suitable material can be pushed into extremely loud broadcast territory while remaining understandable.

That extra authority also brings extra responsibility. Hiss, room tone, fan noise, hum, microphone noise, and other low-level junk can come forward along with the material. If the noise, flattening, or edge becomes more important than the extra loudness, turn Auto-Adapt down.

100% is not a quality score. It is the far end of what Stream is willing to try.

### Bass behavior

Stream's low-frequency control is adaptive. It can add useful foundation to lean material and pull back when the low end is already strong.

The system is deliberately prevented from treating healthy bass as a problem just because Auto-Adapt is high. It is still content-dependent, so use your ears rather than expecting every recording to receive the same amount of low-frequency change.

### Stereo width

Stream can open narrower material as Auto-Adapt increases, but it does not simply turn the Side channel up by a fixed amount. Existing width and program behavior are taken into account.

If a source is already unusually wide, check the result in mono and make sure the extra width is actually helping.

## Podcast Leveler

Podcast Leveler is designed around speech, but it is not speech-only. Its goal is a flatter, more consistent presentation that can make very different voices and sources feel as though they occupy the same sonic space.

At high Auto-Adapt it is allowed to flatten short transients. That can help contain plosives, hard consonants, mic bumps, and other brief peaks while keeping speech understandable. Some listeners will also prefer that denser presentation on music or mixed program material.

Its low-frequency correction is deliberately restrained. Adding a lot of bass to a thin voice can sound helpful until the next plosive arrives with much more low-frequency energy and makes everything later in the chain react harder. Podcast adds body without trying to manufacture a bottom end that the rest of the processor then has to fight.

Podcast does not add stereo widening as Auto-Adapt rises. Input Drive remains independent and starts at +3.5 dB. The automatic level control has up to 14 dB of gain or reduction available internally; there is no separate AGC or width control to set.

Start with the default +3.5 dB Input Drive and listen to the loudest and quietest parts of the recording. Raise or lower Input until the overall fill feels right, then use Auto-Adapt for more control when the material needs it.

If breaths, pauses, room noise, or the beginnings of phrases start sounding unnaturally pulled around, back off Input or Auto-Adapt.

### How much bad leveling will Core rescue?

Podcast has **+/-14 dB of AGC rescue authority** so badly offset material, including a 32-bit floating-point recording that was never normalized, has room to recover. That range is rescue capability, not a promise to erase arbitrarily large differences between hosts or sources.

Core deliberately does not try to smash every source to the same level at any cost. Extreme automatic leveling can drag room noise, mic problems, clipping, and other artifacts to the front. If one source is wildly louder or quieter than the others, improve the source gain structure, get help setting it up, or use a processor designed for extreme leveling and accept its tradeoffs. Core operating at 100% Auto-Adapt will be the closest I ever get to something like that.

If you deliberately want to drive Core harder, another option is to keep Auto-Adapt relatively low and raise Input Drive. That pushes the existing processing harder without redefining high Auto-Adapt as a universal “flatten everything” mode.

### Working through Auto-Adapt

Podcast has three starting points behind the continuous slider: 0%, 50%, and 100%. The lower half develops from fast recovery to a slower, fuller sound. Above 20%, the automatic level control begins taking on more of the work, reaching its middle setting at 50%.

Above 50%, compression, tone, and the level feeding the final stages develop toward the stronger upper voicing. At 70%, Core changes how it divides the audio into bands; at 90%, it brings in an additional high-frequency filter. Those changes reverse when you move back down. They are automatic parts of this Mode, not extra switches you need to find.

They can change the sound audibly, so choose a setting by listening rather than treating Auto-Adapt as a volume fader.

## Smooth Limiter

Smooth Limiter is for material that is already reasonably balanced when you mainly want smooth loudness and peak control.

Smooth Limiter expects you to use **Input Drive**. If the limiter is barely doing anything, turn the input up into it and listen. Input Drive decides how hard the material hits the limiter. Auto-Adapt changes how the limiter responds once it gets there.

Like anything that starts off smooth, this limiter can stay smooth for a surprisingly long time—but if you keep poking at it and driving it beyond what it can tolerate, eventually **it gets an edge**.

That is part of the point. Start conservatively, turn Input Drive up, and listen for the point where the extra loudness and control are still helping more than they are hurting.

Smooth Limiter rewards listening. That is intentional.

## Saved settings and defaults

A fresh instance starts in **Stream polish**, with Input Drive at 0 dB and Auto-Adapt at 0%.

Loading a saved project or preset restores the saved settings. Core should not silently replace your saved Input Drive or Auto-Adapt value with the fresh-instance defaults.

Changing Mode is different: that is an intentional request for another processing setup, so the Mode's normal starting Input Drive is loaded. You can then change Input Drive to whatever you want.

## Output level

Podcast, Stream, and Smooth now share the same final delivery-limiter settings. The limiter uses reconstruction-aware peak detection plus a sustained-event history path, and it is calibrated so the delivered target is approximately **-0.1 dBTP**. Core handles that internally. There is no output-ceiling or limiter-release control to set.

That does not guarantee that every later conversion will remain below -0.1 dBTP. Sample-rate conversion, lossy encoding, another plugin, or other processing after Core can create new peaks. If the final delivery ceiling matters, measure at the end of the actual delivery chain.

## Troubleshooting

If the sound becomes too dense, distorted, or small, reduce Auto-Adapt first. If the processor is still being hit too hard, reduce Input Drive.

If a plugin does not appear in your host, make sure you installed the correct format and architecture, then rescan the host's plugins.

Install the foobar2000 version through **Preferences > Components** using the `.fb2k-component` package. Do not unpack it and copy a DLL manually.

If you have an older project that used the pre-1.5 CLAP version, read the [CLAP upgrade note](../CHANGELOG.md#clap-upgrade-note) before replacing the old plugin.

## Frequently asked questions

### Is there a safe range for Auto-Adapt?

There is a forgiving part of every Mode, but Core does not define one percentage as universally “safe.” Input level, source dynamics, spectral balance, and the selected Mode all change how much authority is useful.

Start lower, listen, and move upward until the processor is doing the job you want. If the sound gets smaller, fuzzy, edgy, over-flattened, or simply less interesting, turn it back down.

For Smooth Limiter in particular, remember that Auto-Adapt is only half of the experiment: Input Drive determines how hard you are actually hitting the limiter.

### For all this Auto-Adapt talk, why is maximum Auto-Adapt distorting my audio? That's not adapting—that's breaking!

At 100%, Stream is deliberately aggressive, but obvious rasp or broadband crunch is not the goal. The upper Stream range is gain-staged so Density is hit less hard while its multiband compressor and limiter take more responsibility; level is restored after Density, and the Final stage handles the remaining peak and loudness pressure.

That does not make 100% universally appropriate. Extremely bright, clipped, noisy, or already-flattened material can still become edgy, over-compressed, or simply less enjoyable when the processor is given maximum authority. If that happens, turn Auto-Adapt down.

A useful distinction is between **aggressive control** and **unwanted breakup**. Flattened transients, audible gain control, or a very dense presentation can be intentional at the top of Stream. A raspy voice, fuzzy consonants, or sustained broadband crunch is a failure case worth avoiding rather than a sound Core needs in order to become loud.

Auto-Adapt is not a quality control where 100% means "best." It is an authority control. Lower settings ask Core to do less; higher settings allow it to become progressively more assertive. The right setting is the one that gives you the result you actually want.

### What's the difference between multiband compression and Density?

Multiband compression breaks the audio into frequency bands, similar to the bands you might find on an EQ—even something as simple as bass and treble.

Each band can then be compressed differently.

That changes the density of the sound: how full it feels, how evenly different parts sit together, and which parts are brought forward.

In OptiLab, Density is the broader result of that process and the other processing that works alongside it.

### Should every higher Auto-Adapt setting always measure louder?

Auto-Adapt moves between complete voicings. In Stream, the stronger starting point blends toward the newer middle setup before the upper range adds more pressure. It is not a calibrated loudness ramp, and more intense does not always mean a higher meter reading.

Core will not add brute-force makeup gain merely to make a meter rise when its protection stages are working hard. On unusually stressed material, a higher setting can occasionally measure a little softer because the processor is preventing a worse failure. Safety wins over forcing a perfectly increasing meter reading on every possible signal.

## The story behind Core

### How the final limiter evolved

Core did not begin with the limiter design in this release. My original reason for making Core was simpler: I did not like the sound I was hearing from a lot of processing tools, and as somebody who tries to keep learning and teaching what I learn, I wanted to understand why and do something about it.

One of the early ideas was to divide the work between saturation and limiting instead of asking one stage to do everything. Saturation could shave the kinds of peaks it handled gracefully, while the limiter handled the remaining level control. I initially thought that division of labor was more unusual than it really was; later I found expensive commercial processors using variations on the same basic idea.

In October 2026, after Core was already released and being used, I encountered chmaha's REAPER JSFX port of the **x42 Digital Peak Limiter**. Its ability to tolerate very heavy gain reduction with a fast nominal release made me look more closely at the x42 design by Fons Adriaensen and Robin Gareus. In particular, its separation of fast peak control from sustained low-frequency/history behavior became an important influence on the research that led to this shared Core limiter.

The limiter in Core is an independent implementation integrated around Core's existing saturation, Density, mode voicing and Auto-Adapt behavior. No x42 source code is included in Core. I still want the influence credited because that is where this research direction came from.

### Which parts of OptiLab Core were Derek, and which parts were AI?

As the AI involved in building it, I can answer that pretty cleanly.

There are really three answers: what Derek did, what we developed together, and what I or other OpenAI coding models contributed on the engineering side. All three matter.

Derek started with the product idea and the listening target. Early on, he identified specific qualities he wanted from very different processing traditions: broadcast processors such as Optimod, mastering tools such as the TC Finalizer, transparent and less-transparent forms of saturation including ideas we discussed around Airwindows, and other hardware and software he had spent years listening to. The goal was never to clone one of them. He was choosing behaviors he valued and asking whether they could coexist in something of our own.

He also set the boundaries: three useful controls instead of an engineering panel, accessibility as a requirement rather than an extra, clear differences between the modes, and a processor that could get aggressive without turning brittle or tearing speech apart. Most importantly, Derek did the listening. He made the final calls when two technically reasonable answers sounded different. A lot of OptiLab exists because he kept saying things like, "No, that got smaller," "the speech is crunchy," "the piano was cleaner before," "that bass balance is right," or occasionally, "That's it. Don't touch it."

A second category is the work we did together. The unusual staging in Core, the way Auto-Adapt coordinates several kinds of processing at once, the division of labor between saturation and limiting, the order in which stronger processing is allowed to mature, and many of the fixes that came from turning a listening complaint into a DSP hypothesis were developed through repeated back-and-forth. Those were not simply Derek dictating code, and they were not me inventing a product in isolation. They came out of the collaboration.

The third category is work that came from me and from various OpenAI coding models. I wrote and revised a large amount of the code, analyzed source and measurements, proposed DSP and architecture changes, built test harnesses, tracked down implementation bugs, compared processing strategies, audited releases, and drafted or revised a lot of the documentation. Other OpenAI models have also been used for implementation passes, independent audits, criticism, and second opinions. Some ideas genuinely originated on the AI side and survived because they worked; others were rejected, sometimes immediately and correctly.

So it would be misleading to say AI merely typed Derek's design, and it would be just as misleading to say AI designed OptiLab Core. Derek brought the product vision, references, ears, constraints, and final authority. The collaboration produced a large part of the architecture. AI contributed substantial engineering and some design ideas of its own.

That is the version I think gives everybody the right amount of credit.

### What is OptiLab Producer, and when will it be released?

OptiLab Producer is the larger version of the OptiLab system I am developing for people who want more control and feedback than Core is meant to provide. Core will remain the deliberately simple version.

When will Producer be released? Short version: who knows?

More useful version: it is real and actively developed, but there are practical decisions to make before a public release makes sense, including distribution, platform builds, signing, support, and cost. A Mac version, if any, is part of that same discussion. There is no release date yet.
