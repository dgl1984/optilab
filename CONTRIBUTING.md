# Contributing

Bug fixes, accessibility improvements, documentation corrections, and host compatibility work are welcome. Discuss changes to the sound, controls, modes, or public API in an issue first.

Follow docs/BUILDING.md. Keep real-time processing free of memory allocation and blocking operations. Keep the three-control interface and accessible keyboard behavior consistent across supported formats. Include reproduction steps and your host/format/architecture when reporting a problem.

Pull requests should describe the user-visible effect and relevant validation.
Follow the [release procedure](RELEASING.md) before publishing. Automated tests
supplement required manual host, accessibility and listening acceptance.

Internal processing parameter IDs are stable: retain existing numbered IDs when
extending the catalogue. Never commit maintainer-only review fixtures, recordings,
validation logs or `dist-private/`. Public tests must run without private fixtures.

See LICENSE and NOTICE for the project's existing terms. Contact info@lanesaudio.com with questions.
