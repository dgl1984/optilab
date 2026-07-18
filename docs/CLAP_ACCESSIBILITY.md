# CLAP Accessibility Contract

OptiLab Core treats accessibility as part of the plug-in contract. A replacement
editor, framework migration, or new native format must preserve both the
host-level parameter path and the custom-editor path described here.

## Independent Access Paths

Every user parameter must be available through the standard CLAP parameter
extension. Mode, Input Drive, and Auto-Adapt therefore require:

- stable parameter IDs
- meaningful names and module text
- correct minimum, maximum, default, and stepped behavior
- value-to-text output with the real unit
- text-to-value parsing where supported
- automation and process flags appropriate to the DSP
- state save and restore independent of editor lifetime

This path allows a host, OSARA, an automation envelope, or another accessibility
layer to operate the plug-in without creating the custom GUI. Never make the
editor the only way to change a parameter.

The custom editor is a second path. It must remain usable with a screen reader
and keyboard even when the host-level parameter list is available.

## Native Editor Controls

Prefer standard Windows controls with native MSAA/UI Automation semantics.
Controls need associated labels, a deterministic Tab and Shift+Tab order,
visible focus, and complete keyboard operation.

Do not assume that a native control exposes the same unit used by the DSP.
Windows trackbars announce their normalized position as a percentage. That is
correct for Auto-Adapt, but it turns an Input Drive value such as `+4.5 dB` into
approximately `56%`. OptiLab keeps the visual Input Drive trackbar for pointer
users and puts keyboard and screen-reader focus on a native read-only field
whose accessible value is the exact signed dB text. Arrow keys change it by
`0.1 dB`, Page Up and Page Down by `1 dB`, and Home and End select its limits.

Meters are supplementary visuals. They must never replace parameter controls,
enter the Tab order, or generate continuous screen-reader announcements.

## REAPER Window Parenting

REAPER can pass a parent HWND whose class is
`reaperPluginHostWrapProc`. Placing the editor directly beneath this wrapper can
leave REAPER's wrapper above the editor for keyboard routing and hit testing.
The result is an interface that looks correct but does not receive Tab or mouse
input until focus is forced into one of its descendants.

For this REAPER-specific parent:

1. Remember the wrapper HWND.
2. Create the accessible editor under the wrapper's parent so the editor and
   wrapper are siblings.
3. Convert the wrapper's screen bounds into the shared parent's client
   coordinates and use those coordinates for editor placement and resizing.
4. Show the editor normally.
5. Post a message rather than hiding the wrapper immediately, because REAPER
   calls `ShowWindow` on its wrapper after the plug-in's `gui.show()` callback.
6. When the posted message runs, hide the wrapper and focus the first logical
   control.
7. Use the normal CLAP-provided parent unchanged in every other host.

The technique was informed by James Teh's GPLv2
[App2Clap project](https://github.com/jcsteh/app2clap). Permanent references:

- [`DS_CONTROL | WS_CHILD` dialogs made from native controls](https://github.com/jcsteh/app2clap/blob/d175ef32c4da31dcc711fa8b8a481c1654987411/src/resource.rc#L13-L46)
- [REAPER wrapper detection, sibling parenting, and deferred hiding](https://github.com/jcsteh/app2clap/blob/d175ef32c4da31dcc711fa8b8a481c1654987411/src/common.cpp#L11-L44)
- [the original explanation of the Tab and hit-testing problem](https://github.com/jcsteh/app2clap/commit/f0af98c852f314c4efc7de8767fe51cc69a92fd2)

OptiLab independently implements this behavior inside its responsive custom
window rather than copying App2Clap's dialog resources.

## Required Regression Coverage

`native/tests/ClapPluginSmokeTests.cpp` creates a fake
`reaperPluginHostWrapProc` hierarchy and requires all of the following:

- the OptiLab editor is a sibling of the wrapper
- the wrapper is hidden after editor show
- Mode receives initial focus
- Tab reaches Input Drive and then Auto-Adapt
- Input Drive exposes a signed dB value through MSAA
- keyboard adjustment changes the accessible dB text
- controls remain standard native combo box, edit, and trackbar classes

Do not weaken or remove these assertions to accommodate a replacement GUI. Make
the replacement GUI satisfy them or add equivalent tests for a demonstrably
better accessibility implementation.

## Fork Checklist

Before releasing a fork or editor rewrite:

- Verify every parameter through the host's generic parameter interface.
- Test the custom editor with NVDA using only the keyboard.
- Open the editor in REAPER and press Tab without first clicking or using object
  navigation.
- Confirm non-percentage controls announce their actual values and units.
- Check mouse hit testing after applying the REAPER wrapper workaround.
- Resize the editor at small and large dimensions and inspect focus visibility.
- Run the CLAP smoke test and retain the rendered-output regression hash.
