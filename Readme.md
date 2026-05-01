<!-- TABLE OF CONTENTS -->
<details>
 <summary>Table of Contents</summary>
 <ol>
   <li><a href="#production-lens-validation-tool">Production Lens Validation Tool</a></li>
   <li><a href="#key-features">Key Features</a></li>
   <li><a href="#our-team">Our Team</a></li>
   <li>
     <a href="#getting-started">Getting Started</a>
     <ul>
        <li>
          <a href="#windows-build-instructions">Windows Build Instructions</a>
              <ul>
                <li><a href="#dependencies">Dependencies</a></li>
                <li><a href="#installation">Installation</a></li>
              </ul>
        </li>
        <li>
          <a href="#ubuntu-build-instructions">Ubuntu Build Instructions</a>
             <ul>
                <li><a href="#dependencies-1">Dependencies</a></li>
                <li><a href="#installation-1">Installation</a></li>
             </ul>
        </li>
     </ul>
   </li>
   <li><a href="#license">License</a></li>
 </ol>
</details>


<!-- PROJECT -->
# Production Lens Validation Tool
An application created for the lens testing team at OptiTrack that takes an image and identifies and grades the relevant lens' quality by examining how circular the markers are.

<img width="1920" height="1080" alt="Lens Tool - Full App - Zoom View" src="https://github.com/natpuck/natpuck.github.io/blob/main/lens_app_screenshot.png" />
**Alt text:** Full view of the application using zoom view and displaying focus and lens statistics.

The program the lens testing team currently uses is intended for direct motion capture usage and is therefore much more technically complex than what's required for basic lens testing. Our program is intended to be a lightweight and easy to use solution for the team, who'll experience the same functionality, but in a much more streamlined way.

<!-- KEY FEATURES -->
# Key Features
* Viewing focus and circularity metrics via graphs and plain numbers
* Clear, color-coded pass/inconclusive/fail lens grading metrics
* Ability to export lens metrics
* Dual-language options (English and Simplified Chinese)
* Mode to zoom in on outer quadrants or specific markers

<!-- OUR TEAM -->
# Our Team
**Daniel Green** (fuzzylogic88) --> Team Captain, Scrum Master
<br>**Nathan Puckett** (natpuck) --> GUI
<br>**Bernardo Mendes** (BernardoM03) --> Implementation
<br>**Jack Ollenbrook** (OllenJ) --> Implementation
<br>**Raed Kabir** (Reptop) --> GUI, Implementation

<!-- GETTING STARTED -->
# Getting Started

## Windows Build Instructions

<!-- DEPENDENCIES -->
### Dependencies

* [Qt](https://www.qt.io/download-dev) 6.10.0 (Important: select the `msvc2022_64` component during installation)
* [OpenCV](https://opencv.org/releases/) 4.12.0
* MSVC2022\_64 C++ toolchain
* [Cmake](https://cmake.org/download/) 4.1.2


<!-- INSTALLATION -->
### Installation

1. **Clone** this repository. *⚠️Avoid placing it too deep in your file system; Windows max-path issues can break the build script.*
2. **Set environment variables (configure-time only)** 
     - `OPENCV_DIR = ..\opencv\build`
     - `OPENCV_BIN_DIR = ..\opencv\build\bin`
     - `Qt6_DIR = ..\Qt\6.10.0\msvc2022_64\lib\cmake\Qt6` 
     - `QT_PLUGIN_PATH = ..\Qt\6.10.0\msvc2022_64\plugins` *Avoid setting `QT_PLUGIN_PATH` globally; deployment handles plugins.*
3. **Build** using the provided script: 
     ```
     ..\Production-Lens-Validation\CameraViewerApp\winBuild.bat
     ```
     Example:*`./WinBuild.bat <CameraSDK_PATH>`*
5. Your executable will be located at `..\Production-Lens-Validation\CameraViewerApp\build\Release\CameraViewerApp.exe`.


## Ubuntu Build Instructions

<!-- DEPENDENCIES -->
### Dependencies

1. Install Dependencies:
   ```
   sudo apt update && sudo apt install -y cmake git build-essential
   qt6-base-dev qt6-base-private-dev qt6-tools-dev qt6-svg-dev
   libgl1-mesa-dev libjpeg-dev libopencv-dev python3-opencv
   ```


<!-- INSTALLATION -->
### Installation

1. Clone Repository
<br>Navigate to the directory where you want the project, then run:
    ```
    git clone https://github.com/fuzzylogic88/Production-Lens-Validation.git
    cd Production-Lens-Validation
    ```

2. Build Project
   ```
   chmod +x build.sh
   ./build.sh ../OptiTrack_Camera_SDK_3.4.1_Final_Ubuntu
   ```

3. (Optional) Enable color camera support (FFmpeg)
    - Install FFmpeg:
       ```
       sudo apt install -y ffmpeg
       ```
    - Build with FFmpeg support:
       ```
       ./linuxBuild.sh ../CameraSDK --ffmpeg
       ```

5. Configure network (required)
<br>Set the camera network interface to Link-Local Only:
<br>`Settings → Network → Wired (camera port) → IPv4 → IPv4 Method → Link-Local Only`

6. Run application
<br>`./build/CameraViewerApp`


<!-- LICENSE -->
# License

Distributed under the GNU LESSER GENERAL PUBLIC LICENSE License.
See `LICENSE.txt` in the "licenses" folder for more information.
Third-party licenses can also be found in the "licenses" folder, named accordingly.

