# Final delivery true-peak reference test

`validate_delivery_truepeak.py` is an optional public reference test for the shared final delivery limiter. It is not part of ordinary CTest because it needs Python, NumPy and SciPy.

After building `optilab-core-cli`, run for example:

```text
python tests/validate_delivery_truepeak.py --cli build/optilab-core-cli --report delivery-truepeak.tsv
```

The test generates deterministic stereo stress material at 8 kHz through 768 kHz, processes Podcast, Stream and Smooth Limiter at 100% Auto-Adapt, reconstructs the output with an independent polyphase resampler, and fails above -0.09 dBTP. The release candidate measured a worst case of approximately -0.11 dBTP on this test.

This is a reference stress test, not a claim that later lossy encoding or sample-rate conversion can never create a higher peak. Measure the actual final delivery chain when a downstream ceiling is contractual.
