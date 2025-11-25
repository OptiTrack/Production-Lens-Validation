<!-- TABLE OF CONTENTS -->
<details>
 <summary>Table of Contents</summary>
 <ol>
   <li><a href="#project">Project</a></li>
   <li><a href="#our-team">Our Team</a></li>
   <li>
     <a href="#getting-started">Getting Started</a>
     <ul>
       <li><a href="#dependecies">Dependecies</a></li>
       <li><a href="#installation">Installation</a></li>
     </ul>
   </li>
   <li><a href="#license">License</a></li>
 </ol>
</details>


<!-- PROJECT -->
# Production Lens Validation Tool
An application that can take an image and identify and grade the relevant lens features by examining how circular a markers is or particular image features. Additionally it would be useful to automate the lens focusing process.


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



<!-- DEPENDECIES -->
### Dependecies


* [Qt](https://www.qt.io/download-dev) 6.10.0 (Important: select the `msvc2022_64` component during installation)
* [OpenCV](https://opencv.org/releases/) 4.12.0
* MSVC2022\_64 C++ toolchain
* [Cmake](https://cmake.org/download/) 4.1.2



### Installation
1. **Clone** this repository. *⚠️Avoid placing it too deep in your file system; Windows max-path issues can break the build script.*
2. **Set environment variables (configure-time only)** 
  - `Qt6_DIR = ..\Qt\6.10.0\msvc2022_64\lib\cmake\Qt6` 
  - `QT_PLUGIN_PATH = ..\Qt\6.10.0\msvc2022_64\plugins` *Avoid setting `QT_PLUGIN_PATH` globally; deployment handles plugins.*
3. **Build** using the provided script: 
  `..\Production-Lens-Validation\CameraViewerApp\winBuild.bat` Example:*`./WinBuild.bat <CameraSDK_PATH>`*
4. Your executable will be located at `..\Production-Lens-Validation\CameraViewerApp\build\Release\CameraViewerApp.exe`.


<!-- LICENSE -->
# License


Distributed under the GNU LESSER GENERAL PUBLIC LICENSE License.
See `LICENSE.txt` in the "licenses" folder for more information.
Third-party licenses can also be found in the "licenses" folder, named accordingly.

