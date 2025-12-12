<!-- TABLE OF CONTENTS -->
<details>
  <summary>Table of Contents</summary>
  <ol>
    <li><a href="#summary">Summary</a></li>
    <li><a href="#our-team">Our Team</a></li>
    <li>
      <a href="#getting-started">Getting Started</a>
      <ul>
        <li><a href="#prerequisites">Prerequisites</a></li>
        <li><a href="#installation">Installation</a></li>
      </ul>
    </li>
    <li><a href="#license">License</a></li>
  </ol>
</details>

<!-- SUMMARY -->
# Summary
Create an application that can take an image and identify and grade the relevant lens features by examining how circular a markers is or particular image features. Additionally it would be useful to automate the lens focusing process.

<img width="1920" height="1080" alt="Lens Tool - Full App - Zoom View" src="https://github.com/user-attachments/assets/28e3603f-9dba-4553-82b2-434cbec8813a" />

<!-- OUR TEAM -->
# Our Team
Daniel Green (Scrum Master) --> greend5@oregonstate.edu
<br>Nathan Puckett --> puckette@oregonstate.edu
<br>Bernardo Mendes --> mendesb@oregonstate.edu
<br>Jack Ollenbrook --> ollenbrj@oregonstate.edu
<br>Raed Kabir --> kabirr@oregonstate.edu

<!-- GETTING STARTED -->
# Getting Started

### Prerequisites

* Qt 6.10.0
* OpenCV 4.12.0
* MSVC2022\_64 C++ compiler

### Installation
1. **Clone** this repository.
2. **Set environment variables (configure-time only)**  
   - `Qt6_DIR = ..\\Qt\\6.10.0\\msvc2022_64\\lib\\cmake\\Qt6`  
   - (Avoid setting `QT_PLUGIN_PATH` globally; deployment handles plugins.)
3. **Build** using the provided script:  
   `CameraSDK/OptiTrackCameraSDK_confidential_115_release-3.4.0_BUILD110/CameraSDK/samples/CameraViewerApp/winBuild.bat`
4. Your executable will be in `.\build\\Release\\CameraViewerApp.exe`.

### Deploy (Qt DLLs)
After a **Release** build, deploy the matching Qt runtime DLLs and plugins next to the executable. This is the current solution for now.

**PowerShell:**
```powershell
& "C:\\Qt\\6.10.0\\msvc2022_64\\bin\\windeployqt.exe" --release --force --compiler-runtime `
  <path to CameraViewerApp.exe> 

```
<!-- LICENSE -->
# License
All non-third party code included in this repository is jointly owned by the team mentioned above and NaturalPoint Inc. DBA OptiTrack. 

See `LICENSE.txt` in the "Documents" folder for more information.

Third-party licenses can be found in the "CameraViewerApp/license" folder, named accordingly.


<p align="right">(<a href="#readme-top">back to top</a>)</p>
