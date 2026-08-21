# Unit tests

Tests for the platform-independent parts of the project. They need **no camera,
no Qt, no OpenCV and no Camera SDK** — only CMake and a C++17 compiler — so they
run identically on a developer machine, on Windows CI and on Ubuntu CI.

## Running them

```bash
cmake -B build -DBUILD_APP=OFF
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The test binary can also be run directly, which is usually quicker while
iterating. It is at `build/bin/Release/focus_core_tests.exe` with Visual Studio
and `build/bin/focus_core_tests` with Make or Ninja:

```bash
./build/bin/focus_core_tests            # everything
./build/bin/focus_core_tests --list     # list test names
./build/bin/focus_core_tests --filter decay
```

Exit codes: `0` all selected tests passed, `1` a test failed, `2` the filter
matched nothing.

## What is covered

`test_focus_score.cpp` covers `focus::FocusScoreModel`
([CameraViewerApp/core/FocusScore.h](../CameraViewerApp/core/FocusScore.h)).

`FocusEvaluator` measures marker-edge sharpness as the standard deviation of a
Laplacian over a mask of bright pixels. That raw figure has no fixed scale — it
depends on the lens, the camera and the exposure — so `FocusScoreModel` turns it
into the 0..1 readout by tracking the blurriest and sharpest values seen this
session. The OpenCV work stays in `FocusEval.cpp`; everything that happens to
the number afterwards is tested here:

| Area | Behaviour under test |
| --- | --- |
| Frame gating | brightness floor, bright-pixel mask threshold, minimum marker pixel count |
| Cold start | bounds begin at their configured values with no score |
| Scoring | mid-range sharpness scores 0.5; a steady scene converges on its raw score |
| Adaptive bounds | a new sharpness peak widens the upper bound by only `boundsAlpha` of the gap, so one transient frame cannot rescale the readout |
| Clamping | scores stay in 0..1, and a narrow bound range cannot blow up the divide |
| Loss of signal | the score fades by `ewmAlpha` per frame and never goes negative, while learned bounds survive |
| Reset | video-mode changes return the model to a cold start |
| Configuration | tuning constants are honoured, including disabling smoothing |

## Adding a test

Add a `TEST(...)` to `test_focus_score.cpp` (or a new file listed in both
`add_executable` and `TEST_SOURCES` in [CMakeLists.txt](CMakeLists.txt)):

```cpp
TEST(descriptive_name_of_the_behaviour) {
    FocusScoreModel model;
    CHECK_NEAR(model.Update(107.5), 0.1, 1e-12);
}
```

CMake discovers every `TEST(...)` at configure time and registers it as its own
CTest test, so a failure names the exact case. Available checks:
`CHECK_TRUE`, `CHECK_FALSE`, `CHECK_EQ`, `CHECK_NEAR`, `CHECK_LE`, `CHECK_GT`.

## Why a hand-rolled harness

[test_support.h](test_support.h) is about 100 lines and has no dependencies.
That keeps CI hermetic — no package manager, no network fetch, nothing to cache
— which matters more here than framework features. Anything that needs
GoogleTest or Catch2 later can be swapped in behind the same CTest entry points.

## Testing code that touches the camera

Logic that needs a `CameraLibrary::Bitmap`, an OpenCV `Mat` or a Qt widget
cannot run here. To make such logic testable, move the decision-making part into
`CameraViewerApp/core/` (dependency free) and leave the image or device handling
in the app layer — the way `FocusEvaluator` now does the OpenCV measurement and
hands a single number to `FocusScoreModel`.

`CircleMarkerDetector::CategorizeShape` is the obvious next candidate: it is
pure threshold logic over a circularity value, currently private to an
OpenCV-dependent class.
