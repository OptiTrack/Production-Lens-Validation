<!-- TABLE OF CONTENTS -->
<details>
  <summary>Table of Contents</summary>
  <ol>
    <li><a href="#project-statement">Project Statement</a></li>
    <li>
      <a href="#project-scope">Getting Started</a>
      <ul>
        <li><a href="#scope">Scope</a></li>
        <li><a href="#stretch-goals">Stretch Goals</a></li>
      </ul>
    </li>
    <li><a href="#design">Design</a></li>
    <li><a href="#success-criteria">Success Criteria</a></li>
    <li><a href="#learning-goals">Learning Goals</a></li>
  </ol>
</details>


<!-- PROJECT STATEMENT -->
# Project Statement
When testing lenses there are circular features to analyze. Those features can come in a few different shapes (circular, ovals, and hook shaped). Circular shapes are ideal, ovals have a certain “circularity” threshold that they must meet to be acceptable, hook shaped features are unacceptable. 

The scope of this project is to make an application that can take an image provided by the OptiTrack Camera SDK, identify what category the features fall into, give the image an overall score, categorize that score into “Pass”, “Review”, or “Fail” categories. Extra work such as identifying when a lens is focused will likely also be part of the scope of this project. This can be done by analyzing the edges of an image to give a score on the thickness of the edge. 
Stretch Scope
Other stretch goals exist for this project as well such as identifying when a lens is focused. This can be done by analyzing the edges of an image to give a score on the thickness of the edge. This goal could be combined with some mechanical tools to make getting the lens into focus as fast as possible. 
 
Other tasks that make the process as quick and easy as possible might also be possible to increase throughput and decrease the complexity for the operator.  

<img width="620" height="480" alt="Lens Tool - Full App - Zoom View" src="https://github.com/user-attachments/assets/28e3603f-9dba-4553-82b2-434cbec8813a" />

<!-- PROJECT SCOPE -->
# Project Scope

## Scope and Deliverables

1. Create an application that evaluates image data from our Camera SDK in a specific way to evaluate whether a lens is good, questionable, or bad quality.
2. Camera appears in the sample application
3. Qt based user interface for controlling the application.
4. Settings can be changed to control image quality. Duplex mode, MJPEG, and Object modes required.
5. Option to segment the image into center and corners only.
6. Option to overlay marker metrics (size, circularity, etc.)
7. Design a scoring mechanism for whether a lens passes or fails the evaluation and report that in the application. Red/yellow/green lights that indicate quality.
8. Multilingual support for English and Chinese.
9. New features like edge detection to automate the lens focusing procedure.

## Stretch Goals

10. Automatic gain feature.
11. Other user experience improvements.

## Out of Scope (Don’t do)
* Doing any work in Motive. This is an all Camera SDK application. 

# Design 

Below are some images that help explain what is expected from the look and feel of the application.

## Definition
![Lens Test - Anatomy 2](https://github.com/user-attachments/assets/3f342437-ec72-470d-bdac-38e236bda62f)
<img width="1920" height="838" alt="Lens Testing - Error States 1" src="https://github.com/user-attachments/assets/729e55ad-8f5a-4baf-aeb4-a7d93419448b" />

## Use Case Examples
<img width="1920" height="1080" alt="Lens Tool - Full App - No Camera 1" src="https://github.com/user-attachments/assets/6d636960-4a29-47e6-a44e-fa393c225422" />
<img width="1920" height="1080" alt="Lens Tool - Full App - Wide View 1" src="https://github.com/user-attachments/assets/1e2accc1-b0c0-4e00-afe3-4d3d13af58ac" />
<img width="1920" height="1080" alt="Lens Tool - Full App - Zoom View 1" src="https://github.com/user-attachments/assets/a07c2ec5-7788-4c0f-b47d-a0bcc2c39437" />
<img width="1920" height="1080" alt="Lens Tool - Full App - Focus Tool 1" src="https://github.com/user-attachments/assets/bbababe2-6e27-4af9-8110-e9bf07813b65" />

# Success Criteria

At minimum, a successful project would include the ability to visualize a camera using a 5 segment view, be able to adjust properties to make the image looks good, and the ability to correctly categorize different lenses.

This will be used for internal testing and at our lens manufacturer to ensure the quality of lenses before they ship internationally. Detecting bad batches of lenses saves both sides money. Simplifying the process reduces error and how much knowledge is required to run the process. 

# Learning Goals 

Since this is aimed at being a student project it’s important to have some learning goals associated with the project. Those are roughly summarized below. 

* Learn how to interface with camera hardware through an SDK.
* Apply machine vision techniques to analyze images.
* User interface programming and design.
* Learn how to establish requirements and stakeholders for a project to validate progress.
* Complete a small useful application for helping to automate a lengthy and skill-based process.
* Possibly gain some cross collaboration experience with the Mechanical Engineering team to improve surrounding processes.  
