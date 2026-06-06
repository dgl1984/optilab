# OptiLab Core

OptiLab Core is a free JSFX audio processor for REAPER.

It was inspired by the kind of processing used in broadcast chains, mastering limiters, and other tools that help shape finished audio. It is not meant to be a clone of one specific piece of hardware or software. The goal was to take some of those ideas — leveling, density, control, saturation, limiting, and polish — and make them easier to reach from a simple plugin.

OptiLab Core has three main controls: **Mode**, **Input**, and **Auto-Adapt**.

That simplicity is intentional.

Sometimes I have to stack compressor and limiter plugins to get the sound I want. That can work, but it can also be annoying when I just want to get to a useful result quickly. OptiLab Core came from wanting something simpler that I could reach for during normal work.

It does not offer audio restoration tools, and it is not meant to magically fix damaged or badly recorded audio. But for podcasts, streams, voice work, music playback, and quick mastering-style polish, it can be a simple way to add control, texture, and a more finished sound without building a large processor chain.

## A few practical notes

OptiLab Core is intentionally simple. It is not a cut-down toy, and it is not trying to replace every processor you own. The idea is to give you a small number of controls that can still lead to a finished, useful sound.

Start gently. A little input drive can go a long way.

If the sound becomes too flat, too dense, or too pushed, turn **Input** down before changing everything else.

## Quick start

1. Choose a Mode.
2. Switching modes will adjust the Input to a good starting value for that mode.
3. Raise or lower Input until the processor feels active but not strained.
4. Adjust Auto-Adapt if you want more leveling, density, tone control, or protection.


## What it does

OptiLab Core brings leveling, density, tone control, saturation, and limiting together in a way that can be shaped from three main controls:

* Leveling for voice and mixed program material
* Broadcast-style density and control
* Smooth limiting and peak handling
* Bass and high-frequency shaping and protection
* Simple control that does more than it may appear to at first

The goal is not just loudness. The goal is useful control that still sounds musical.

## The controls

### Mode

Choose the general job you want OptiLab Core to do.

**Podcast Leveler**
Designed for speech, podcasting, voice tracks, and mixed voice content. This mode focuses on leveling, control, and a smoother mid-focused sound.

**Stream Polish**
Designed for music, internet radio, and higher-energy material. This mode gives a more finished, broadcast-style sound without requiring a lot of setup.

**Smooth Limiter**
Designed for cleaner limiting and mastering-style use where you want control without turning the whole processor into an aggressive broadcast chain.

### Input

Input controls how hard the processor is driven.

This is the first control to adjust. If the sound is too flat, too dense, or too pushed, turn Input down. If the processor is barely changing anything, turn Input up.

### Auto-Adapt

Auto-Adapt changes the way OptiLab Core balances leveling, tone, density, and protection.

It adjusts several parts of the processor at once, depending on the selected Mode. As it is turned up, OptiLab Core can become more active with leveling, low-frequency control, high-frequency protection, density, and peak handling.

One of the more interesting parts of this kind of processing is that the sound is not treated as one flat block. The midrange is used as an important anchor, because that is where so much of the clarity, voice, and musical presence lives. The processor can then adjust gain behavior, low-frequency weight, and high-frequency control in relation to that midrange energy.

That means the lows and highs are not simply boosted or limited in isolation. They are shaped so they can sit better around the middle of the sound. This can help keep bass from overwhelming the rest of the material, help highs stay smoother when the processor is working harder, and let the whole result feel more controlled without just crushing everything.

That idea was one of the most interesting things I discovered when looking into how classic broadcast-style processing worked. Before that, I thought of limiting mostly as catching peaks. There is much more to it than that.

Lower Auto-Adapt settings stay closer to the base Mode. Higher settings allow the processor to work harder and adapt more aggressively.

A good starting workflow is:

1. Choose the Mode.
2. Adjust Input until the processor sounds right.
3. Use Auto-Adapt if you want more control, density, or protection.

## Installation

OptiLab Core is a JSFX plugin for REAPER.

Download the latest release, then copy the `.jsfx` file into your REAPER Effects folder.

To find the correct folder, open REAPER and go to:

**Options > Show REAPER resource path in explorer/finder...**

Once that folder opens, go into the **Effects** folder and copy the OptiLab Core `.jsfx` file there.

After that, restart REAPER if it is already running, then add OptiLab Core from REAPER’s FX browser like any other JSFX effect.

## Download

The latest version will be available from the GitHub Releases section for this repository.

## Donations and contact

OptiLab Core is free.

If it helps your station, stream, podcast, music work, or mastering workflow, donations are welcome and appreciated. They are not required.

I would rather people actually use the processor than lock it behind a checkout page. If you like it and want to support more development, you can donate through PayPal:

[Donate via PayPal](https://paypal.me/dgl1984)

For questions, bug reports, or feedback, contact:

`info@lanesaudio.com`

## License and modifications

OptiLab Core is released under the Apache License 2.0.

You may use it, modify it, and share modified versions under the terms of that license.

If you make improvements, fixes, presets, accessibility changes, or useful tweaks, I would genuinely like to hear about them. I may be able to incorporate good changes into the main version so everyone benefits.

Please send feedback, bug reports, or suggested changes to:

`info@lanesaudio.com`

If you share a modified version publicly, please make it clear that it has been modified from the original OptiLab Core release.
