# Testing OptiLab Core

OptiLab Core ships public, reproducible tests so another developer can verify the released engine with deterministic fixtures and generated signals.

The public test set is intentionally similar in spirit to Tone Trace and Space Trace: small deterministic fixtures, direct engine tests, wrapper-contract checks, and one clearly documented stress test for behavior that matters at the product boundary.

## 1. Native public regression suite

Configure a build with testing enabled, build it, then run CTest.

On a generic CMake host where JUCE plug-ins and Winamp are not required:

```text
cmake -S native -B build-public -DBUILD_TESTING=ON -DOPTILAB_BUILD_JUCE_PLUGINS=OFF -DOPTILAB_BUILD_WINAMP=OFF -DOPTILAB_BUILD_FOOBAR2000=OFF
cmake --build build-public --config Release
ctest --test-dir build-public -C Release --output-on-failure
```

On Windows, `BUILD_WINDOWS.bat` is the normal release-oriented path. It builds the supported formats and runs the public native regression tests as part of the release flow.

The public CTest suite covers:

- Core mode/default/state behavior;
- public DSP trajectory invariants;
- WAV-file handling used by the CLI;
- Winamp integer-PCM delivery behavior;
- fixed delivery/latency behavior;
- Windows wrapper contracts when those targets are available;
- JUCE wrapper behavior when the JUCE target is available.

A passing CTest run means the tested public contracts still match the released source. It does not replace host loading, accessibility, or listening checks.

## 2. JSFX parity and state validation

On Windows, run:

```text
VALIDATE_JSFX.bat
```

For the extended matrix:

```text
VALIDATE_JSFX.bat -Thorough
```

The JSFX gate uses a pinned ysfx runtime and generated signals. It compares the JSFX implementation with native Core across coordinator states, sample rates, drive settings, live updates, saved-state recall, and legacy slider-only session behavior.

See `docs/JSFX.md` for the exact matrix, tolerances, and remaining REAPER acceptance requirements.

## 3. Delivery true-peak stress test

`tests/validate_delivery_truepeak.py` independently checks the shared final delivery limiter. It generates deterministic stereo stress material from 8 kHz through 768 kHz, processes all three modes at 100% Auto-Adapt, reconstructs inter-sample peaks independently, and fails if the measured result exceeds the published threshold.

After building `optilab-core-cli`:

```text
python tests/validate_delivery_truepeak.py --cli build-public/optilab-core-cli --report delivery-truepeak.tsv
```

Python, NumPy, and SciPy are required for this optional reference test.

See `tests/DELIVERY_TRUEPEAK.md` for scope and interpretation.

## 4. What is deliberately not public

The public tests cover the behavior required to build and maintain Core. Release review also uses maintainer-only listening material and additional regression fixtures that are not part of the public source package.

Those private assets are not required to build Core or run the public suite. Their absence is expected in a public checkout.

## 5. Release acceptance

Passing automated tests is necessary, not sufficient. Before publication, the maintainer release gate also requires the supported Windows builds, JSFX runtime validation, actual host loading, accessibility checks, and listening acceptance described in `RELEASING.md`.
