# Contributing Guide

How to set up, code, test, review, and release so contributions meet our Definition of Done.

## Code of Conduct

Reference the project/community behavior expectations and reporting process.

## Getting Started

PREREQUISITES: Qt 6.10.0, OpenCV 4.12.0, MSVC2022\_64 C++ compiler

1. Clone repository onto local system  
2. Set environment variables  
   1. Qt6\_DIR \= ..\\Qt\\6.10.0\\msvc2022\_64\\lib\\cmake\\Qt6  
   2. QT\_PLUGIN\_PATH \= ..\\Qt\\6.10.0\\msvc2022\_64\\plugin  
3. Run winBuild.bat at CameraSDK/OptiTrackCameraSDK\_confidential\_115\_ release-3.4.0\_BUILD110/CameraSDK/samples/CameraViewerApp  
4. Run CameraViewerApp.exe inside \\build\\Releases  
   1. If .dll errors, copy required .dll files from ..\\Qt\\6.10.0\\msvc2022\_64\\bin, ..\\opencv\\build\\bin\\ , and ..\\opencv\\build\\x64\\vc16\\bin\\ into exe folder

## Branching & Workflow

1. Checkout the “develop” branch and pull the most recent changes  
2. Branch off of develop (good to include the issue number '\#XX' in the branch name)  
3. Work on your branch until the feature is implemented or the bug is fixed and commit your changes

## Issues & Planning

New issues outside of those provided by the project partners can be added to the repo [here](https://github.com/fuzzylogic88/Production-Lens-Validation/issues), and will be estimated, triaged, and assigned during sprint planning meetings.

## Commit Messages

Commit messages include brief summary/notes of changes made to code since last commit.

## Code Style, Linting & Formatting

No code style rules are in place for this project, and no linter has been dictated. 

## Testing

The unit test suite lives in [`tests/`](../tests) and covers the platform-independent focus scoring math in `CameraViewerApp/core/`. It needs no camera, Qt, OpenCV or Camera SDK — only CMake and a C++17 compiler:

```
cmake -B build -DBUILD_APP=OFF
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

* CI runs this suite on both Windows and Ubuntu for every push and pull request; it must pass before a PR is merged.
* Coverage of tested components has a goal threshold of 75%.
* New tests are created at the discretion of the developer. See [`tests/README.md`](../tests/README.md) for what is covered, how to add a case, and the pattern used to make camera-facing logic testable: keep the decision-making in `CameraViewerApp/core/` and the device or image handling in the app layer.

## Pull Requests & Reviews

Once feature is implemented from the branch you are working on, you are ready to create a PR

1. Create a pull request to pull your changes into the develop branch  
2. Wait for a reviewer to approve the PR  
   1. The reviewer will do a code review and check whether all the required steps are completed (documentation, tests, etc…)  
3. Merge changes to develop once a reviewer has approved your changes

Template of PR  
\[Issue \#: Name of the feature\]  
\[Brief summary of changes within this PR\]

- [ ] Documentation complete  
- [ ] Test files: \[Include test files here, or N/A if not applicable\]

## CI/CD

Two GitHub Actions workflows live in [`.github/workflows/`](../.github/workflows).

### CI — [`ci.yml`](../.github/workflows/ci.yml)

Runs on **every push to any branch**, on every pull request against `develop`/`main`, and on demand from the Actions tab. Before merging anything into `develop`, make sure CI is green.

| Job | Platform | What it does |
| --- | --- | --- |
| `Unit tests` | Ubuntu **and** Windows | Configures with `-DBUILD_APP=OFF`, builds the test suite and runs it through CTest. Failures are printed in the job log. |
| `Build app (Windows)` | Windows | Installs Qt and OpenCV, builds the application with MSVC against `CameraSDK/`, runs the tests, deploys the Qt runtime with `windeployqt`, and uploads a runnable package. |
| `Build app (Ubuntu)` | Ubuntu | Installs the packages from `UbuntuBuildInstructions.txt`, builds against `OptiTrack_Camera_SDK_3.4.1_Final_Ubuntu/`, runs the tests, and uploads a package. |

Both application jobs build from the repository root `CMakeLists.txt`, which produces the executable in `build/bin/<Config>/`. `winBuild.bat` and `build.sh` still work exactly as before.

### Getting the build for a commit

Every commit you push produces a downloadable build. Open the **Actions** tab, pick the run for your commit, and download from the **Artifacts** section at the bottom of the run summary:

| Artifact | Contents |
| --- | --- |
| `CameraViewerApp-windows-x64-build<N>` | The Windows application with its Qt, OpenCV and Camera SDK runtime files |
| `CameraViewerApp-linux-x64-build<N>` | The Ubuntu build, including `libCameraLibrary.so` |

`<N>` is the workflow run number, the same one shown next to the run in the Actions tab. Each package also contains a `BUILD_INFO.txt` naming the commit, branch and run it came from, so an artifact can always be traced back to a specific commit. Artifacts are kept for 7 days; adjust `retention-days` in the workflow to change that.

The application artifact is uploaded before the test step, so a commit still produces a usable build when a test fails — the job is still reported as failed. Note that a Windows package is roughly 200 MB, mostly `CameraLibrary.dll`, so pushing many commits in a day uses a fair amount of the repository's Actions storage.

### CD — [`release.yml`](../.github/workflows/release.yml)

Builds and publishes release packages for both platforms. See **Release Process** below.

### Build options

| Option | Default | Purpose |
| --- | --- | --- |
| `BUILD_APP` | `ON` | Build the GUI application (needs Qt 6, OpenCV, Camera SDK). |
| `BUILD_TESTS` | `ON` | Build the unit test suite. |
| `ENABLE_FFMPEG` | `OFF` | Build the FFmpeg video decoder path. |
| `ENABLE_ASAN` | `OFF` | Build with AddressSanitizer. |

### Keeping the workflows working

* The Qt and OpenCV versions CI installs are pinned in the `env:` block at the top of each workflow. Change them there when the project moves to a new version, and update the prerequisites above to match.
* Dependabot proposes updates to the actions themselves monthly ([`dependabot.yml`](../.github/dependabot.yml)).

## Security & Secrets

* – No external APIs are planned for inclusion in this project, so the risk of secret leakage (API keys, passwords, personal information) is minimal.  
* \- Pull requests can be vetted for inclusion of sensitive information manually by the approver, and blocked if violations are noted.  
* \- Vulnerabilities can be reported by the observer to the broader team, where corrective actions can be taken to remediate (update packages, fix code, etc).   
* \- There are no plans currently to implement automated vulnerability scanning tools for this software.

## Documentation Expectations

* As new features are added/finalized, update the README to mention these new features  
  * Include any necessary instructions, tips, etc. on how to utilize these features  
* Leave comments throughout your code briefly explaining what certain functions do  
  * If you think any lines or chunks of code would be difficult to understand for someone reviewing or working on your code, make sure to leave comments there too.

## Release Process

Releases are tagged with [semantic versioning](https://semver.org/) (`vMAJOR.MINOR.PATCH`) and published by the [`release.yml`](../.github/workflows/release.yml) workflow. The project is currently at **0.8.0**.

### Where the version lives

The [`VERSION`](../VERSION) file at the repository root is the single source of truth. Everything else reads it:

| Consumer | How it is used |
| --- | --- |
| `cmake/ProjectVersion.cmake` | Parses it and exposes `PLV_VERSION*`, plus the short commit hash |
| Root `CMakeLists.txt` | `project(... VERSION ${PLV_VERSION})` |
| `CameraViewerApp/Version.h.in` | Generated into `Version.h` as `PLV_VERSION_STRING` / `PLV_VERSION_FULL` |
| `main.cpp` | `QCoreApplication::applicationVersion()` and the window title |
| `CameraViewerApp/version.rc.in` | Windows executable file properties (right-click → Details) |
| CI | Recorded in each artifact's `BUILD_INFO.txt` |
| `release.yml` | Refuses to publish a tag that disagrees with it |

Builds from a git checkout report `0.8.0+<short sha>`; the bare `0.8.0` is used where only the release version matters. The hash is resolved when CMake configures, so re-run CMake to refresh it.

### Cutting a release

1. Make sure `develop` is green in CI and merged into `main`.
2. Bump [`VERSION`](../VERSION) and move the [`CHANGELOG.md`](../CHANGELOG.md) *Unreleased* entries under the new version, then commit.
3. Tag the release commit and push the tag:

```
git tag v0.8.1
git push origin v0.8.1
```

4. The workflow checks the tag against `VERSION`, builds both platforms, runs the unit tests (a failing test blocks the release), and publishes a GitHub Release containing `CameraViewerApp-<version>-windows-x64.zip` and `CameraViewerApp-<version>-linux-x64.tar.gz`, each with a SHA256 checksum.

Running the workflow manually from the Actions tab produces the same packages as downloadable artifacts without publishing a release — useful for a dry run.

## Support & Contact

Primary maintainer: Daniel Green  
Contact (email): [greend5@oregonstate.edu](mailto:greend5@oregonstate.edu)   
Github profile: [fuzzylogic88 (Daniel Green)](https://github.com/fuzzylogic88)  
Typical response time: 1-2 business days

Questions can be asked in the discussion section of the repo [here](http://fuzzylogic88/Production-Lens-Validation%20·%20Discussions%20·%20GitHub).  
