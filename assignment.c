#ifdef USEGLEW
#include <GL/glew.h>
#endif

//Cross platform includes  
#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif
//Standard libraries
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdarg.h>

//Lorenz Parameters

double s  = 10;
double b  = 2.6666;
double r  = 28;

int th = 0;   // azimuth angle 
int ph = 15;  // elevation angle

//Convenience routine to output raster text
// Code taken from ex7.c  reference
#define LEN 8192
void Print(const char* format, ...)
{
   char    buf[LEN];
   char*   ch = buf;
   va_list args;
   va_start(args, format);
   vsnprintf(buf, LEN, format, args);
   va_end(args);
   while (*ch)
      glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *ch++);
}

//This function displays the Lorenz attractor and axes
void display()
{
   //Erase the window and the depth buffer 
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   //Reset transformations
   glLoadIdentity();

   //Rotate the scene based on view angles
   glRotated(ph, 1, 0, 0);
   glRotated(th, 0, 1, 0);

   //Scale down the attractor to fit in view
   glScaled(0.05, 0.05, 0.05);

   //Draw the Lorenz attractor as a line
   double x = 1, y = 1, z = 1;
   double dt = 0.001;
   int i;

   glBegin(GL_LINE_STRIP);

   for (i = 0; i < 50000; i++) // number of points to draw
   {
      //Color based on position along trajectory 
      glColor3f(0.5 + 0.5*sin(i*0.001),
                0.5 + 0.5*sin(i*0.001 + 2),
                0.5 + 0.5*sin(i*0.001 + 4));
      glVertex3d(x, y, z);

      // Lorenz equations
      double dx = s*(y - x);
      double dy = x*(r - z) - y;
      double dz = x*y - b*z;
      x += dt*dx;
      y += dt*dy;
      z += dt*dz;
   }
   glEnd();

   // Draw axes
   glLineWidth(2.0);
   glBegin(GL_LINES);
   
   // X axis - red
   glColor3f(1, 0, 0);
   glVertex3d(0, 0, 0);
   glVertex3d(30, 0, 0);
   
   // Y axis - green
   glColor3f(0, 1, 0);
   glVertex3d(0, 0, 0);
   glVertex3d(0, 30, 0);

   // Z axis - blue
   glColor3f(0, 0, 1);
   glVertex3d(0, 0, 0);
   glVertex3d(0, 0, 30);
   glEnd();
   glLineWidth(1.0);

   // Label axes
   glColor3f(1, 0, 0);
   glRasterPos3d(30, 0, 0);
   Print("X");
   glColor3f(0, 1, 0);
   glRasterPos3d(0, 30, 0);
   Print("Y");
   glColor3f(0, 0, 1);
   glRasterPos3d(0, 0, 30);
   Print("Z");

   // Display parameters on screen
   glColor3f(1, 1, 1);
   glWindowPos2i(5, 45);
   Print("s=%.2f  b=%.2f  r=%.2f", s, b, r);
   glWindowPos2i(5, 25);
   Print("Angle=%d,%d", th, ph);
   glWindowPos2i(5, 5);
   Print("Arrows:Rotate  s/S:Sigma  b/B:Beta  r/R:Rho  q:Quit");

   //Flush and swap buffers
   glFlush();
   glutSwapBuffers();
}


// keyboard inputs
// Code taken from ex7.c  reference
void key(unsigned char ch, int x, int y)
{
   if (ch == 27 || ch == 'q')
      exit(0);
   else if (ch == 's')
      s -= 0.5;
   else if (ch == 'S')
      s += 0.5;
   else if (ch == 'b')
      b -= 0.1;
   else if (ch == 'B')
      b += 0.1;
   else if (ch == 'r')
      r -= 0.5;
   else if (ch == 'R')
      r += 0.5;
   else if (ch == '0')
   {
      s = 10;
      b = 2.6666;
      r = 28;
      th = 0;
      ph = 15;
   }
 
   glutPostRedisplay();
}


// arrow keys to rotate view
// Code taken from ex7.c  reference
void special(int key, int x, int y)
{
   if (key == GLUT_KEY_RIGHT)
      th += 5;
   else if (key == GLUT_KEY_LEFT)
      th -= 5;
   else if (key == GLUT_KEY_UP)
      ph += 5;
   else if (key == GLUT_KEY_DOWN)
      ph -= 5;

   th %= 360;
   ph %= 360;

   glutPostRedisplay();
}

// Maintain aspect ratio when window is resized
// Code taken from ex7.c  reference
void reshape(int width, int height)
{
   
   glViewport(0, 0, width, height);
   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   const double dim = 2.0;
   double asp = (height > 0) ? (double)width/height : 1;
   glOrtho(-asp*dim, +asp*dim, -dim, +dim, -dim, +dim);
   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();
}

// Main Function generated with AI assistance 
int main(int argc, char* argv[])
{
   
   glutInit(&argc, argv);
   glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
   glutInitWindowSize(800, 600);
   glutCreateWindow("Lorenz Attractor - Prithvikiran");

#ifdef USEGLEW
   if (glewInit() != GLEW_OK)
   {
      fprintf(stderr, "Error initializing GLEW\n");
      return 1;
   }
#endif

   glutDisplayFunc(display);
   glutReshapeFunc(reshape);
   glutKeyboardFunc(key);
   glutSpecialFunc(special);

   glEnable(GL_DEPTH_TEST);

   printf("Lorenz Attractor Controls:\n");
   printf("  Arrow keys : rotate view\n");
   printf("  s/S        : decrease/increase sigma\n");
   printf("  b/B        : decrease/increase beta\n");
   printf("  r/R        : decrease/increase rho\n");
   printf("  0          : reset all parameters\n");
   printf("  q/ESC      : quit\n");

   glutMainLoop();

   return 0;
}