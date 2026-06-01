README - Assignment 1: Lorenz Attractor
Student: Prithvikiran
========================================

DESCRIPTION
-----------
This program renders the Lorenz Attractor in 3D using OpenGL and GLUT.
The attractor is drawn as a colored line strip with XYZ axes for reference.
Colors shift smoothly along the trajectory based on position.

BUILD INSTRUCTIONS
------------------
To build the program, run:

   make

This compiles assignment.c using gcc with GLEW and freeglut on Windows (MinGW).
The executable will be named Assignment-1.

To run the program:

   ./Assignment-1

To clean build files:

   make clean

REQUIREMENTS
------------
- freeglut
- GLEW
- OpenGL
- libm

KEY BINDINGS
------------
Arrow Keys     : Rotate view (azimuth and elevation)
s / S          : Decrease / Increase sigma
b / B          : Decrease / Increase beta
r / R          : Decrease / Increase rho
0              : Reset all parameters to defaults
q / ESC        : Quit the program

LORENZ PARAMETERS (defaults)
-----------------------------
Sigma : 10
Rho   : 28
Beta  : 2.6667

NOTES
-----
- The scene displays immediately on startup with the full attractor visible.
- Lighting and axes are enabled by default.
- The window title displays the current parameter values.
- Parameters can be adjusted interactively using the key bindings above.

CODE REUSE
----------
- Print() function and keyboard/reshape/navigation code adapted from ex7.c
  provided in course examples.
- Main loop structure references course example code.

TIME SPENT
----------
Approximately 4 hours.

REFERENCES
----------
- Course example ex7.c
- Claude Code 
