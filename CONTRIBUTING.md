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

* Unit tests may be implemented at a later date to ensure repeatability of lens evaluation.  
* Coverage of tested components has a goal threshold of 75%.  
* New tests are created at the discretion of the developer.

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

* Since OptiTrack’s camera SDK is built using CMake, our repo contains a CMake (single-platform) CI workflow that will automatically make test builds of our code  
* Before merging anything to the develop branch, make sure your code passes the CMake workflow’s tests first  
  * NOTE: as of writing this (11/2/25), we’re still working out some bugs with the workflow, so every test fails at the moment, but once it’s finished, make sure to follow the above

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

No versioning scheme has been implemented yet, but might be when the GitHub CMake workflow is functioning.

## Support & Contact

Primary maintainer: Daniel Green  
Contact (email): [greend5@oregonstate.edu](mailto:greend5@oregonstate.edu)   
Github profile: [fuzzylogic88 (Daniel Green)](https://github.com/fuzzylogic88)  
Typical response time: 1-2 business days

Questions can be asked in the discussion section of the repo [here](http://fuzzylogic88/Production-Lens-Validation%20·%20Discussions%20·%20GitHub).  
