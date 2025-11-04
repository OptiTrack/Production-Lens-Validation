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

*  Qt 6.10.0
* OpenCV 4.12.0
* MSVC2022\_64 C++ compiler

### Installation

1. Clone repository onto local system  
2. Set environment variables  
   1. Qt6\_DIR \= ..\\Qt\\6.10.0\\msvc2022\_64\\lib\\cmake\\Qt6  
   2. QT\_PLUGIN\_PATH \= ..\\Qt\\6.10.0\\msvc2022\_64\\plugin  
3. Run winBuild.bat at CameraSDK/OptiTrackCameraSDK\_confidential\_115\_ release-3.4.0\_BUILD110/CameraSDK/samples/CameraViewerApp  
4. Run CameraViewerApp.exe inside \\build\\Releases  
   1. If .dll errors, copy required .dll files from ..\\Qt\\6.10.0\\msvc2022\_64\\bin, ..\\opencv\\build\\bin\\ , and ..\\opencv\\build\\x64\\vc16\\bin\\ into exe folder


<!-- LICENSE -->
# License

Distributed under the ___ License. 
See `LICENSE.txt` in the "licenses" folder for more information.
Third party licenses can also be found in the "licenses" folder named accordingly.


<p align="right">(<a href="#readme-top">back to top</a>)</p>
