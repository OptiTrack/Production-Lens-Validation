# Changelog

All notable changes to Production Lens Validation are recorded here.

The version lives in the [`VERSION`](VERSION) file at the repository root and is
the single source of truth: CMake reads it, the application reports it, the
Windows executable carries it in its file properties, and the release workflow
refuses to publish a tag that disagrees with it. Versions follow
[semantic versioning](https://semver.org/) — `MAJOR.MINOR.PATCH`.

## [Unreleased]

## [0.8.0] - 2026-08-20

First versioned release. The application has been in use before this point; this
is where version tracking starts, ahead of a 1.0 once the lens validation
workflow is signed off.

### Added

- Version system: `VERSION` file, generated `Version.h`, version in the window
  title, in `QCoreApplication::applicationVersion()`, and in the Windows
  executable's file properties.
- CI on Windows and Ubuntu: unit tests, full application builds, and a
  downloadable package for every commit.
- Release workflow: pushing a `v*` tag publishes packages for both platforms
  with SHA256 checksums.
- Unit test suite for the focus scoring math (`tests/`), runnable without a
  camera, Qt, OpenCV or the Camera SDK.

### Changed

- Status readouts (focus result, lens grade, metric values) now take their
  colours from `Designs/motive.css` instead of inline stylesheets, so the same
  state always looks the same. Good is Motive's accent cyan, caution amber,
  failure red.
- Focus scoring math extracted into `CameraViewerApp/core/` so it can be tested
  independently of the camera pipeline. Scoring behaviour is unchanged.

### Fixed

- Missing standard library and Qt includes that broke the GCC/Ubuntu build.
- OpenCV detection with Visual Studio versions newer than the OpenCV release,
  which previously failed with "no binaries compatible with your configuration".

[Unreleased]: https://github.com/OptiTrack/Production-Lens-Validation/compare/v0.8.0...develop
[0.8.0]: https://github.com/OptiTrack/Production-Lens-Validation/releases/tag/v0.8.0
