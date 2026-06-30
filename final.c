// Final Project - "Groundskeeper"  Prithvikiran Premkumar - CSCI 5229

#include "CSCIx229.h"
#include <time.h>

/* =================================================================
 *  CONSTANTS
 * ================================================================= */
#define FIELD        9.0    /* Half-width of the yard           */
#define LR           9.35   /* Radius of the sun orbit arc       */
#define TERRAIN_N    72     /* Terrain mesh resolution (NxN quads)    */
#define MOW_N        72     /* Mowing grid resolution (NxN cells)     */
#define MAX_PARTS    150   /* Maximum active GPU particles (Grass Clippings)           */
#define NUM_CIRCLE_OBS 9    /* Circular collision obstacles            */
#define NUM_BOX_OBS    2    /* Box collision obstacles                 */
#define MOWER_RADIUS   0.55 /* Approximate mower footprint radius      */
#define FLAG_NX 16          /* Flag mesh segments along length         */
#define FLAG_NY 10          /* Flag mesh segments along height         */

/* =================================================================
 *  VIEW / PROJECTION STATE  [hw3, extended with modes 3-5 and inspector]
 * ================================================================= */
int    mode  = 1;   /* 0=ortho 1=persp 2=walk 3=follow 4=seat */
int    axes  = 0;
int    th    = 35;
int    ph    = 30;
int    fov   = 55;
double asp   = 1;
double dim   = 12.0;

/* First-person camera  */
double Ex = 0, Ey = 1.3, Ez = 7;
double fpTh = 0, fpPh = -8;

/* Lighting / sun  */
int    light   = 1;
int    move    = 1;
int    ntex    = 1;
int    fogOn   = 0;      /* fog toggle -- off by default so scene is visible on startup */
float  zh      = 90;     /* Sun angle -- start at noon for best first view */
float  sunElev = 0;      /* User sun-elevation offset */

/*  GLSL shader + particle toggles +++ */
int    useShader    = 1; /* GLSL per-pixel terrain shader */
int    useParticles = 1; /* GPU particle system toggle    */

/* flag / sprinkler / selection / help / fog state */
int    helpOn       = 0;  /* Help overlay toggle ('h' key) */
double sprinklerAng = 0;  /* Sprinkler head rotation angle */
int    sprinklerOn  = 1;  /* Sprinkler on/off ('k' key)    */
int    selObj       = -1; /* Object ID under mouse (-1 = none) */
int    fogMode      = 1;  /* 0=GL_EXP  1=GL_EXP2  2=GL_LINEAR  (';' key) */
int    pickMouseX   = 0, pickMouseY = 0; /* Last mouse position for picking */
double mistLandX    = 0, mistLandZ = 0;  /* Sprinkler water landing point   */

/* Inspector  */
int    inspector   = 0;
int    inspObj     = 0;
int    showNormals = 0;
float  inspAngle   = 0;

#define NUM_INSP_OBJS 10

/* Mower state */
double mowerX   = 0;
double mowerZ   = 2;
double mowerAng = 0;
double mowerSpd = 0;
double steerAng = 0;
double bladeRot = 0;

/* Textures */
unsigned int tex_grass, tex_wood, tex_metal, tex_plastic, tex_rubber;
unsigned int tex_stone, tex_roof, tex_bark, tex_leaf;
unsigned int tex_hedge, tex_gravel, tex_dirt;

/* Display lists  [new]  (Course concept: Display Lists) */
unsigned int dl_fence;

/*  mowing state grid  */
int mowGrid[MOW_N][MOW_N]; /* 0=uncut, 1=cut */

/* GLSL shaders  */
int shader_terrain  = 0; /* Original terrain per-pixel lighting shader  */
int shader_particle = 0; /* GPU particle physics shader                 */
int shader_flag     = 0; /* Per-pixel cloth shading shader for flag      */
int shader_rain     = 0; /* GPU rain particle shader                     */
int terrainMode     = 0; /* 0=Dry  1=Moderate  2=Wet  ('r' key cycles)  */

/*  " RAIN " system  */

#define MAX_RAIN     15000 /* This is the number which controls the number of raindrops in the scene */
#define RAIN_FIELD   9.0
typedef struct { float x, y, z, phase; } RainDrop;
RainDrop rainDrops[MAX_RAIN];
int  rainOn      = 0;  /* toggled by 'y' key or auto in Wet mode */
int  rainInited  = 0;  /* spawn positions generated once         */

/*  Particle system - GRASS CLIPPINGS 
 *  The vertex shader (particle.vert) computes each particle's position
 *  using the kinematic equation:  p = p0 + v*t + 0.5*g*t^2
 *  The CPU stores only spawn data (position, velocity, birth time) and
 *  manages birth/death.  No per-frame CPU position updates.
 */
typedef struct {
   float x, y, z;      /* Spawn position                  */
   float vx, vy, vz;   /* Initial velocity                */
   float birthTime;    /* Elapsed time when spawned (s)   */
   float maxLife;      /* Total lifespan (s)              */
} GPUParticle;
GPUParticle particles[MAX_PARTS];
int   numParticles = 0;
float particleTime = 0; /* Elapsed time -- passed to vertex shader */


/* =================================================================
 *  TEXTURE HELPER  [hw3]
 * ================================================================= */
static void Texture(unsigned int t)
{
   if (ntex) { glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, t); }
   else        glDisable(GL_TEXTURE_2D);
}

/* =================================================================
 *  SHADER UTILITIES  
 *  ReadText, PrintShaderLog, PrintProgramLog, CreateShader,
 *  CreateShaderProg reference example ex25.c.
 *  These compile and link the GLSL shaders used for the terrain
 *  and GPU particle system.
 * ================================================================= */
static char* ReadText(const char* file)
{
   char* buffer;
   /* "rb" (binary) avoids Windows \r\n ftell/fread mismatch  [new fix] */
   FILE* f = fopen(file, "rb");
   if (!f) Fatal("Cannot open text file %s\n", file);
   fseek(f, 0, SEEK_END);
   int n = ftell(f);
   rewind(f);
   buffer = (char*)malloc(n + 1);
   if (!buffer) Fatal("Cannot allocate %d bytes for text file %s\n", n+1, file);
   if (fread(buffer, n, 1, f) != 1)
      Fatal("Cannot read %d bytes for text file %s\n", n, file);
   buffer[n] = 0;
   fclose(f);
   return buffer;
}
static void PrintShaderLog(int shader, const char* file)
{
   int len = 0;
   glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
   if (len > 1) {
      int n = 0;
      char* buffer = (char*)malloc(len);
      if (!buffer) Fatal("Cannot allocate %d bytes for shader log\n", len);
      glGetShaderInfoLog(shader, len, &n, buffer);
      fprintf(stderr, "%s:\n%s\n", file, buffer);
      free(buffer);
   }
   glGetShaderiv(shader, GL_COMPILE_STATUS, &len);
   if (!len) Fatal("Error compiling %s\n", file);
}
static void PrintProgramLog(int prog)
{
   int len = 0;
   glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
   if (len > 1) {
      int n = 0;
      char* buffer = (char*)malloc(len);
      if (!buffer) Fatal("Cannot allocate %d bytes for program log\n", len);
      glGetProgramInfoLog(prog, len, &n, buffer);
      fprintf(stderr, "%s\n", buffer);
      free(buffer);
   }
   glGetProgramiv(prog, GL_LINK_STATUS, &len);
   if (!len) Fatal("Error linking program\n");
}
static int CreateShader(GLenum type, const char* file)
{
   int shader = glCreateShader(type);
   char* source = ReadText(file);
   glShaderSource(shader, 1, (const char**)&source, NULL);
   free(source);
   fprintf(stderr, "Compile %s\n", file);
   glCompileShader(shader);
   PrintShaderLog(shader, file);
   return shader;
}
static int CreateShaderProg(const char* VertFile, const char* FragFile)
{
   int prog = glCreateProgram();
   int vert = CreateShader(GL_VERTEX_SHADER,   VertFile);
   int frag = CreateShader(GL_FRAGMENT_SHADER, FragFile);
   glAttachShader(prog, vert);
   glAttachShader(prog, frag);
   glLinkProgram(prog);
   PrintProgramLog(prog);
   return prog;
}

/* =================================================================
 *  PRIMITIVE BUILDERS 
 * ================================================================= */

/*
 *  Textured unit cube centred at origin.
 *  Geometry from hw2; normals and tex-coords added for hw3 using
 *  the per-face layout of ex15.  Tex coords scaled by size so the
 *  texture tiles at approximately 1 tile per world unit.  [hw3+ex15]
 */
static void CubeTex(double sx, double sy, double sz,
                    float r, float g, float b)
{
   glColor3f(r, g, b);
   glBegin(GL_QUADS);
   /* Top (+Y) */
   glNormal3f(0, +1, 0);
   glTexCoord2d(0,0);    glVertex3d(-0.5,+0.5,-0.5);
   glTexCoord2d(0,sz);   glVertex3d(-0.5,+0.5,+0.5);
   glTexCoord2d(sx,sz);  glVertex3d(+0.5,+0.5,+0.5);
   glTexCoord2d(sx,0);   glVertex3d(+0.5,+0.5,-0.5);
   /* Bottom (-Y) */
   glNormal3f(0, -1, 0);
   glTexCoord2d(0,0);    glVertex3d(-0.5,-0.5,-0.5);
   glTexCoord2d(sx,0);   glVertex3d(+0.5,-0.5,-0.5);
   glTexCoord2d(sx,sz);  glVertex3d(+0.5,-0.5,+0.5);
   glTexCoord2d(0,sz);   glVertex3d(-0.5,-0.5,+0.5);
   /* Front (+Z) */
   glNormal3f(0, 0, +1);
   glTexCoord2d(0,0);    glVertex3d(-0.5,-0.5,+0.5);
   glTexCoord2d(sx,0);   glVertex3d(+0.5,-0.5,+0.5);
   glTexCoord2d(sx,sy);  glVertex3d(+0.5,+0.5,+0.5);
   glTexCoord2d(0,sy);   glVertex3d(-0.5,+0.5,+0.5);
   /* Back (-Z) */
   glNormal3f(0, 0, -1);
   glTexCoord2d(0,0);    glVertex3d(+0.5,-0.5,-0.5);
   glTexCoord2d(sx,0);   glVertex3d(-0.5,-0.5,-0.5);
   glTexCoord2d(sx,sy);  glVertex3d(-0.5,+0.5,-0.5);
   glTexCoord2d(0,sy);   glVertex3d(+0.5,+0.5,-0.5);
   /* Right (+X) */
   glNormal3f(+1, 0, 0);
   glTexCoord2d(0,0);    glVertex3d(+0.5,-0.5,+0.5);
   glTexCoord2d(sz,0);   glVertex3d(+0.5,-0.5,-0.5);
   glTexCoord2d(sz,sy);  glVertex3d(+0.5,+0.5,-0.5);
   glTexCoord2d(0,sy);   glVertex3d(+0.5,+0.5,+0.5);
   /* Left (-X) */
   glNormal3f(-1, 0, 0);
   glTexCoord2d(0,0);    glVertex3d(-0.5,-0.5,-0.5);
   glTexCoord2d(sz,0);   glVertex3d(-0.5,-0.5,+0.5);
   glTexCoord2d(sz,sy);  glVertex3d(-0.5,+0.5,+0.5);
   glTexCoord2d(0,sy);   glVertex3d(-0.5,+0.5,-0.5);
   glEnd();
}

/*  Placed, rotated, scaled textured box */
static void Box(double x, double y, double z,
                double dx, double dy, double dz, double ry,
                float r, float g, float b)
{
   glPushMatrix();
   glTranslated(x, y, z);
   glRotated(ry, 0, 1, 0);
   glScaled(dx, dy, dz);
   CubeTex(dx, dy, dz, r, g, b);
   glPopMatrix();
}

/*
 *  Truncated-cone tube with caps.
 *  Wall normal: analytical outward normal of a truncated cone,
 *  N = (h*cos(a), rb-rt, h*sin(a)), so tapered walls light correctly.
 *  Cap tex mapping uses radial layout from ex20.  [hw3 + ex20]
 */
static void Tube(double rb, double rt, double h, int n, double urep,
                 float r, float g, float b)
{
   glColor3f(r, g, b);
   /* Curved wall -- analytical cone normal */
   glBegin(GL_QUAD_STRIP);
   for (int i = 0; i <= n; i++)
   {
      double a = 360.0*i/n, c = Cos(a), s = Sin(a), u = urep*i/n;
      glNormal3d(h*c, rb-rt, h*s);
      glTexCoord2d(u, 0); glVertex3d(rb*c, 0, rb*s);
      glTexCoord2d(u, 1); glVertex3d(rt*c, h, rt*s);
   }
   glEnd();
   /* Top cap -- radial tex mapping from ex20 */
   glNormal3d(0, 1, 0);
   glBegin(GL_TRIANGLE_FAN);
   glTexCoord2d(0.5, 0.5); glVertex3d(0, h, 0);
   for (int i = 0; i <= n; i++)
   {
      double a = 360.0*i/n;
      glTexCoord2d(0.5*Cos(a)+0.5, 0.5*Sin(a)+0.5);
      glVertex3d(rt*Cos(a), h, rt*Sin(a));
   }
   glEnd();
   /* Bottom cap  */
   glNormal3d(0, -1, 0);
   glBegin(GL_TRIANGLE_FAN);
   glTexCoord2d(0.5, 0.5); glVertex3d(0, 0, 0);
   for (int i = n; i >= 0; i--)
   {
      double a = 360.0*i/n;
      glTexCoord2d(0.5*Cos(a)+0.5, 0.5*Sin(a)+0.5);
      glVertex3d(rb*Cos(a), 0, rb*Sin(a));
   }
   glEnd();
}

/*  Wheel helper -- used for all four mower wheels */
static void Wheel(double x, double z, double radius, double width)
{
   Texture(tex_rubber);
   glPushMatrix();
   glTranslated(x, radius, z);
   glRotated(90, 0, 0, 1);
   glTranslated(0, -width/2, 0);
   Tube(radius, radius, width, 16, 6.0, 0.15f, 0.15f, 0.15f);
   glPopMatrix();
}

/* =================================================================
 *  PARAMETRIC TERRAIN  
 *  y = f(x,z) with gentle undulations.  Normals computed analytically
 *  from the partial derivatives of the height function.
 *  ( Parametric Surfaces, Transformation of Normals )
 * ================================================================= */

/*  Height function: sum of three sinusoidal terms */
static double TerrainHeight(double x, double z)
{
   return 0.15*sin(x*0.5)*cos(z*0.4)
        + 0.08*sin(x*0.9 + z*0.7)
        + 0.05*cos(x*1.3 - z*0.5);
}

/*  Analytical normal = normalize(-dh/dx, 1, -dh/dz)
 *  Computed from the partial derivatives of TerrainHeight().   */
static void TerrainNormal(double x, double z,
                          double *nx, double *ny, double *nz)
{
   double dhdx =  0.15*0.5*cos(x*0.5)*cos(z*0.4)
               +  0.08*0.9*cos(x*0.9 + z*0.7)
               -  0.05*1.3*sin(x*1.3 - z*0.5);
   double dhdz = -0.15*0.4*sin(x*0.5)*sin(z*0.4)
               +  0.08*0.7*cos(x*0.9 + z*0.7)
               +  0.05*0.5*sin(x*1.3 - z*0.5);
   double len = sqrt(dhdx*dhdx + 1 + dhdz*dhdz);
   *nx = -dhdx/len;  *ny = 1.0/len;  *nz = -dhdz/len;
}

/*  Draw terrain mesh.  Colour reflects cut/uncut */
static void DrawTerrain(void)
{
   Texture(tex_grass);
   double step = 2.0*FIELD / TERRAIN_N;
   for (int i = 0; i < TERRAIN_N; i++)
   {
      for (int j = 0; j < TERRAIN_N; j++)
      {
         double x0 = -FIELD + i*step, z0 = -FIELD + j*step;
         double x1 = x0 + step,       z1 = z0 + step;
         /* Mow-grid colour: cut cells are lighter  */
         int mi = i*MOW_N/TERRAIN_N, mj = j*MOW_N/TERRAIN_N;
         if (mi >= MOW_N) mi = MOW_N-1;
         if (mj >= MOW_N) mj = MOW_N-1;
         int cut = mowGrid[mi][mj];
         if (cut)          glColor3f(0.55f, 0.78f, 0.45f);
         else if ((i+j)%2) glColor3f(0.50f, 0.72f, 0.42f);
         else              glColor3f(0.35f, 0.60f, 0.30f);
         double nnx, nny, nnz;
         glBegin(GL_QUADS);
           TerrainNormal(x0, z0, &nnx, &nny, &nnz);
           glNormal3d(nnx, nny, nnz);
           glTexCoord2d(x0*0.5+FIELD, z0*0.5+FIELD);
           glVertex3d(x0, TerrainHeight(x0, z0), z0);
           TerrainNormal(x1, z0, &nnx, &nny, &nnz);
           glNormal3d(nnx, nny, nnz);
           glTexCoord2d(x1*0.5+FIELD, z0*0.5+FIELD);
           glVertex3d(x1, TerrainHeight(x1, z0), z0);
           TerrainNormal(x1, z1, &nnx, &nny, &nnz);
           glNormal3d(nnx, nny, nnz);
           glTexCoord2d(x1*0.5+FIELD, z1*0.5+FIELD);
           glVertex3d(x1, TerrainHeight(x1, z1), z1);
           TerrainNormal(x0, z1, &nnx, &nny, &nnz);
           glNormal3d(nnx, nny, nnz);
           glTexCoord2d(x0*0.5+FIELD, z1*0.5+FIELD);
           glVertex3d(x0, TerrainHeight(x0, z1), z1);
         glEnd();
      }
   }
}

/* =================================================================
 *  SCENE OBJECTS
 * ================================================================= */

/*  House with roof, door and windows */
static void House(void)
{
   Texture(tex_stone);
   Box(0, 1.5, 0, 4.0, 3.0, 3.5, 0, 0.85f, 0.80f, 0.70f);
   Texture(tex_wood);
   Box(0.8, 0.75, 1.76, 0.8, 1.5, 0.06, 0, 0.50f, 0.30f, 0.15f);
   Texture(tex_metal);
   Box(-0.9, 1.8, 1.76, 0.7, 0.7, 0.06, 0, 0.6f, 0.75f, 0.9f);
   Box(0.8,  1.8, -1.76, 0.7, 0.7, 0.06, 0, 0.6f, 0.75f, 0.9f);
   Texture(tex_roof);
   glColor3f(0.65f, 0.25f, 0.20f);
   glBegin(GL_QUADS);
     /* Front roof slope -- normal computed from 45 deg angle: (0, 0.707, 0.707) */
     glNormal3f(0, 0.7071f, 0.7071f);
     glTexCoord2d(0,0); glVertex3d(-2.2, 3.0,  1.75);
     glTexCoord2d(4,0); glVertex3d(+2.2, 3.0,  1.75);
     glTexCoord2d(4,2); glVertex3d(+2.2, 4.2,  0);
     glTexCoord2d(0,2); glVertex3d(-2.2, 4.2,  0);
     /* Back roof slope -- normal (0, 0.707, -0.707) */
     glNormal3f(0, 0.7071f, -0.7071f);
     glTexCoord2d(0,0); glVertex3d(+2.2, 3.0, -1.75);
     glTexCoord2d(4,0); glVertex3d(-2.2, 3.0, -1.75);
     glTexCoord2d(4,2); glVertex3d(-2.2, 4.2,  0);
     glTexCoord2d(0,2); glVertex3d(+2.2, 4.2,  0);
   glEnd();
   glBegin(GL_TRIANGLES);
     glNormal3f(1, 0, 0);
     glTexCoord2d(0,0);   glVertex3d(+2.0, 3.0, +1.75);
     glTexCoord2d(2,0);   glVertex3d(+2.0, 3.0, -1.75);
     glTexCoord2d(1,1.5); glVertex3d(+2.0, 4.2,  0);
     glNormal3f(-1, 0, 0);
     glTexCoord2d(0,0);   glVertex3d(-2.0, 3.0, -1.75);
     glTexCoord2d(2,0);   glVertex3d(-2.0, 3.0, +1.75);
     glTexCoord2d(1,1.5); glVertex3d(-2.0, 4.2,  0);
   glEnd();
}

/*  Garden shed */
static void Shed(void)
{
   Texture(tex_wood);
   Box(0, 1.0, 0, 2.0, 2.0, 1.8, 0, 0.60f, 0.42f, 0.25f);
   Texture(tex_metal);
   Box(0, 2.05, 0, 2.3, 0.10, 2.0, 0, 0.50f, 0.50f, 0.52f);
   Texture(tex_wood);
   Box(0, 0.65, 0.91, 0.6, 1.3, 0.05, 0, 0.40f, 0.25f, 0.12f);
}

/*  Tree: tapered trunk + three stacked conical canopy layers  [new]
 *  Uses hw3 Tube() with the same analytical cone normal. */
static void Tree(double trunkH, double canopyR, double canopyH)
{
   Texture(tex_bark);
   Tube(0.15, 0.10, trunkH, 12, 2.0, 0.50f, 0.35f, 0.18f);
   Texture(tex_leaf);
   for (int layer = 0; layer < 3; layer++)
   {
      double base = trunkH - 0.3 + layer*canopyH*0.35;
      double r    = canopyR * (1.0 - layer*0.2);
      glPushMatrix();
      glTranslated(0, base, 0);
      Tube(r, 0.05, canopyH*0.55, 16, 3.0, 0.20f, 0.55f+layer*0.05f, 0.15f);
      glPopMatrix();
   }
}

/*  Hedge section  */
static void HedgeSection(double len)
{
   Texture(tex_hedge);
   Box(0, 0.5, 0, len, 1.0, 0.6, 0, 0.22f, 0.50f, 0.18f);
}

/*  Garden bed with small flower tubes  [new] */
static void GardenBed(double len, double wid)
{
   Texture(tex_dirt);
   Box(0, 0.06, 0, len, 0.12, wid, 0, 0.45f, 0.30f, 0.18f);
   Texture(tex_leaf);
   for (double t = -len/2+0.3; t < len/2; t += 0.6)
   {
      glPushMatrix();
      glTranslated(t, 0.12, 0);
      Tube(0.08, 0.06, 0.25, 8, 1.0, 0.80f, 0.30f, 0.40f);
      glPopMatrix();
   }
}

/*  Gravel path with polygon offset to avoid z-fighting with terrain
 *  (Course concept: Polygons / Polygon Offset)  */
static void GravelPath(double x0, double z0, double x1, double z1,
                       double width)
{
   Texture(tex_gravel);
   double dx = x1-x0, dz = z1-z0;
   double len = sqrt(dx*dx + dz*dz);
   double ang = atan2(dx, dz) * 180.0 / 3.14159265;
   glPushMatrix();
   glTranslated((x0+x1)/2, 0.02, (z0+z1)/2);
   glRotated(ang, 0, 1, 0);
   glEnable(GL_POLYGON_OFFSET_FILL);
   glPolygonOffset(-1, -1);
   glNormal3f(0, 1, 0);
   glColor3f(0.65f, 0.60f, 0.50f);
   glBegin(GL_QUADS);
   glTexCoord2d(0,0);       glVertex3d(-width/2, 0, -len/2);
   glTexCoord2d(width,0);   glVertex3d(+width/2, 0, -len/2);
   glTexCoord2d(width,len); glVertex3d(+width/2, 0, +len/2);
   glTexCoord2d(0,len);     glVertex3d(-width/2, 0, +len/2);
   glEnd();
   glDisable(GL_POLYGON_OFFSET_FILL);
   glPopMatrix();
}

/*  Dustbin */
static void Dustbin(float r, float g, float b)
{
   Texture(tex_plastic);
   Tube(0.30, 0.36, 0.80, 24, 2.0, r, g, b);
   Tube(0.36, 0.40, 0.06, 24, 2.0, r*0.7f, g*0.7f, b*0.7f);
   glPushMatrix(); glTranslated(0, 0.86, 0);
   Tube(0.42, 0.22, 0.12, 24, 2.0, r*0.85f, g*0.85f, b*0.85f);
   glPopMatrix();
   Texture(tex_metal);
   Box(+0.40, 0.55, 0, 0.10, 0.06, 0.18, 0, r*0.6f, g*0.6f, b*0.6f);
   Box(-0.40, 0.55, 0, 0.10, 0.06, 0.18, 0, r*0.6f, g*0.6f, b*0.6f);
}

/*  Fence -- posts placed at TerrainHeight() to fix floating bug  [hw3+new fix]
 *  The original hw3 fence placed posts at a fixed y=0 regardless of
 *  terrain.  Calling TerrainHeight() at each post position ensures they
 *  follow the undulating ground instead of floating above dips. */
static void Fence(void)
{
   Texture(tex_wood);
   double e = FIELD - 0.3, hy;
   for (double t = -e; t <= e+0.01; t += 1.5)
   {
      hy = TerrainHeight(+e, t); Box(+e, hy+0.45, t, 0.12,0.9,0.12, 0, 0.66f,0.50f,0.32f);
      hy = TerrainHeight(-e, t); Box(-e, hy+0.45, t, 0.12,0.9,0.12, 0, 0.66f,0.50f,0.32f);
      hy = TerrainHeight(t, +e); Box(t, hy+0.45, +e, 0.12,0.9,0.12, 0, 0.66f,0.50f,0.32f);
      hy = TerrainHeight(t, -e); Box(t, hy+0.45, -e, 0.12,0.9,0.12, 0, 0.66f,0.50f,0.32f);
   }
   /* Horizontal rails -- at a fixed height since they span many posts */
   Box(+e, 0.75, 0, 0.08, 0.10, 2*e, 0, 0.72f, 0.55f, 0.36f);
   Box(-e, 0.75, 0, 0.08, 0.10, 2*e, 0, 0.72f, 0.55f, 0.36f);
   Box(0, 0.75, +e, 2*e, 0.10, 0.08, 0, 0.72f, 0.55f, 0.36f);
   Box(0, 0.75, -e, 2*e, 0.10, 0.08, 0, 0.72f, 0.55f, 0.36f);
}

/* =================================================================
 *  DRIVABLE RIDING MOWER  
 *  Steerable front wheels, spinning blade, headlight geometry.
 * ================================================================= */
static void DrawMower(void)
{
   double wr = 0.17, ww = 0.12;
   double h = TerrainHeight(mowerX, mowerZ);
   glPushMatrix();
   glTranslated(mowerX, h, mowerZ);
   glRotated(mowerAng, 0, 1, 0);
   /* Rear wheels (fixed axle)   */
   Wheel(-0.50, -0.34, wr, ww);
   Wheel(+0.50, -0.34, wr, ww);
   /* Front left wheel (steerable)   */
   glPushMatrix();
   glTranslated(-0.50, wr, +0.34); glRotated(steerAng, 0,1,0);
   glTranslated(0, -wr, 0); Texture(tex_rubber);
   glPushMatrix(); glRotated(90,0,0,1); glTranslated(0,-ww/2,0);
   Tube(wr,wr,ww,16,6.0, 0.15f,0.15f,0.15f); glPopMatrix(); glPopMatrix();
   /* Front right wheel (steerable)   */
   glPushMatrix();
   glTranslated(+0.50, wr, +0.34); glRotated(steerAng, 0,1,0);
   glTranslated(0, -wr, 0); Texture(tex_rubber);
   glPushMatrix(); glRotated(90,0,0,1); glTranslated(0,-ww/2,0);
   Tube(wr,wr,ww,16,6.0, 0.15f,0.15f,0.15f); glPopMatrix(); glPopMatrix();
   /* Body / deck   */
   Texture(tex_metal);
   Box(0, 0.30, 0, 1.20, 0.22, 0.92, 0, 0.85f, 0.16f, 0.12f);
   /* Engine cowling   */
   Box(-0.10, 0.52, 0.10, 0.46, 0.30, 0.52, 0, 0.32f, 0.32f, 0.34f);
   /* Exhaust  */
   glPushMatrix(); glTranslated(0.16, 0.66, 0.10);
   Tube(0.10, 0.08, 0.18, 12, 1.0, 0.55f, 0.18f, 0.18f); glPopMatrix();
   /* Handlebars   */
   glPushMatrix(); glTranslated(0, 0.34, -0.42); glRotated(-52, 1,0,0);
   Box(-0.45, 0.55, 0, 0.06, 1.25, 0.06, 0, 0.32f, 0.32f, 0.35f);
   Box(+0.45, 0.55, 0, 0.06, 1.25, 0.06, 0, 0.32f, 0.32f, 0.35f);
   Box(0, 1.16, 0, 1.02, 0.08, 0.08, 0, 0.28f, 0.28f, 0.28f);
   glPopMatrix();
   /* Grass catcher   */
   Texture(tex_plastic);
   Box(0, 0.30, -0.62, 0.85, 0.40, 0.30, 0, 0.20f, 0.45f, 0.20f);
   /* Spinning blade   */
   Texture(tex_metal);
   glPushMatrix(); glTranslated(0, 0.08, 0.15); glRotated(bladeRot, 0,1,0);
   Box(0, 0, 0, 0.80, 0.04, 0.10, 0, 0.60f, 0.60f, 0.62f); glPopMatrix();
   /* Headlight  [new] */
   Texture(tex_plastic);
   Box(0, 0.40, 0.50, 0.20, 0.12, 0.06, 0, 0.95f, 0.90f, 0.70f);
   glPopMatrix();
}

/*  mowing mechanic 
 *  Marks the 3x3 grid of cells around the mower's current position as
 *  cut.  Called every frame while the mower is moving.  Cut cells are
 *  drawn lighter in DrawTerrain() and also handled in the terrain shader.
 */
static void UpdateMowing(void)
{
   double cellSize = 2.0*FIELD / MOW_N;
   int ci = (int)((mowerX + FIELD) / cellSize);
   int cj = (int)((mowerZ + FIELD) / cellSize);
   for (int di = -1; di <= 1; di++)
      for (int dj = -1; dj <= 1; dj++) {
         int mi = ci+di, mj = cj+dj;
         if (mi >= 0 && mi < MOW_N && mj >= 0 && mj < MOW_N)
            mowGrid[mi][mj] = 1;
      }
}


/*  WAVING FLAG  - PER PIXEL SHADER + ANALYTICAL NORMALS 
 *  A flat cloth grid whose vertices are displaced by a travelling sine
 *  wave: z_offset = amplitude * sin(x*frequency - time*speed).  The
 *  wave is largest at the free edge (x=flagLen) and zero at the pole
 *  (x=0), so the flag looks pinned at the staff.
 *
 *  Normals are computed analytically each frame from the partial
 *  derivative of the wave with respect to x (the cloth bends only
 *  along x so dz/dy=0), exactly the same analytical-normal technique
 *  used for the terrain.  This demonstrates Transformation of Normals
 *  on a MOVING parametric surface rather than a static one.
 *  (Course concepts: Parametric Surfaces, Transformation of Normals)
 */
static void Flag(double t)
{
   double flagLen = 1.2, flagH = 0.7;
   double amp = 0.10, freq = 5.0, speed = 4.0;

   /* Activate per-pixel cloth shader for the flag   */
   if (useShader && shader_flag)
   {
      glUseProgram(shader_flag);
      /* Enable two-sided lighting so back face shades differently  [new] */
      glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, 1);
   }
   Texture(tex_plastic);
   glColor3f(0.80f, 0.10f, 0.10f);

   for (int i = 0; i < FLAG_NX; i++)
   {
      double x0 = flagLen * i     / FLAG_NX;
      double x1 = flagLen * (i+1) / FLAG_NX;
      /* Wave amplitude tapers to 0 at the pole so the cloth looks pinned */
      double amp0 = amp * (x0/flagLen);
      double amp1 = amp * (x1/flagLen);

      glBegin(GL_QUAD_STRIP);
      for (int j = 0; j <= FLAG_NY; j++)
      {
         double y  = flagH * j / FLAG_NY;
         double z0 = amp0 * sin(freq*x0 - speed*t);
         double z1 = amp1 * sin(freq*x1 - speed*t);

         /* Analytical slope dz/dx = d/dx[amp(x)*sin(freq*x - speed*t)]  */
         double dzdx0 = (amp/flagLen)*sin(freq*x0-speed*t)
                      + amp0*freq*cos(freq*x0-speed*t);
         double dzdx1 = (amp/flagLen)*sin(freq*x1-speed*t)
                      + amp1*freq*cos(freq*x1-speed*t);

         /* Surface normal = normalize(-dz/dx, 0, 1) in flag local frame */
         double len0 = sqrt(dzdx0*dzdx0 + 1.0);
         double len1 = sqrt(dzdx1*dzdx1 + 1.0);

         glNormal3d(-dzdx0/len0, 0, 1.0/len0);
         glTexCoord2d(x0/flagLen, y/flagH); glVertex3d(x0, y, z0);
         glNormal3d(-dzdx1/len1, 0, 1.0/len1);
         glTexCoord2d(x1/flagLen, y/flagH); glVertex3d(x1, y, z1);
      }
      glEnd();
   }
   /* Deactivate flag shader and restore single-sided lighting   */
   if (useShader && shader_flag)
   {
      glUseProgram(0);
      glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, 0);
   }
}

/*  Flagpole + waving flag, placed near the house */
static void FlagPole(double t)
{
   Texture(tex_metal);
   Tube(0.04, 0.03, 2.4, 10, 1.0, 0.70f, 0.70f, 0.72f);
   /* Small finial ball on top */
   glPushMatrix(); glTranslated(0, 2.4, 0);
   Tube(0.06, 0.005, 0.10, 10, 1.0, 0.85f, 0.70f, 0.20f); glPopMatrix();
   /* Flag mounted just below the top */
   glPushMatrix(); glTranslated(0.03, 2.05, 0);
   Flag(t); glPopMatrix();
}


/*  sprinkler with Bezier water arc 
 *  The sprinkler head rotates slowly.  Each frame the water stream is
 *  drawn as a 1D Bezier curve (course concept: Parametric Curves) from
 *  the nozzle to a landing point on the ground, using glMap1d /
 *  glEvalMesh1 .
 *
 *  The arc DISABLED when:
 *    (a) sprinklerOn == 0  (user pressed 'k' to turn it off), OR
 *    (b) terrainMode == 0  (Dry -- a dry yard wouldn't have a sprinkler ON)
 
 */
static void SprinklerArc(double baseX, double baseY, double baseZ, double ang)
{
   /* Four Bezier control points: P0=nozzle, P1/P2=arc controls, P3=landing */
   double reach = 1.6, rad = ang * 3.14159265/180.0;
   double lx = baseX + reach*sin(rad);
   double lz = baseZ + reach*cos(rad);

   GLfloat P[4][3] = {
      { (float)baseX,                (float)baseY,         (float)baseZ },
      { (float)(baseX+0.35*sin(rad)), (float)(baseY+0.55), (float)(baseZ+0.35*cos(rad)) },
      { (float)(baseX+0.95*sin(rad)), (float)(baseY+0.45), (float)(baseZ+0.95*cos(rad)) },
      { (float)lx,                   0.02f,                (float)lz }
   };

   /* Draw anti-aliased Bezier arc  [ex35 + ex32] */
   glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_2D);
   glEnable(GL_LINE_SMOOTH); glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
   glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glColor4f(0.65f, 0.80f, 1.0f, 0.85f);
   glLineWidth(2.0f);
   glEnable(GL_MAP1_VERTEX_3);
   glMap1f(GL_MAP1_VERTEX_3, 0.0, 1.0, 3, 4, (GLfloat*)P);
   glMapGrid1f(20, 0.0, 1.0);
   glEvalMesh1(GL_LINE, 0, 20);
   glDisable(GL_MAP1_VERTEX_3);
   glDisable(GL_LINE_SMOOTH); glDisable(GL_BLEND);
   glLineWidth(1.0f);
   if (light) glEnable(GL_LIGHTING);

   
}

/*  Sprinkler base + rotating head  */
static void Sprinkler(void)
{
   Texture(tex_plastic);
   Tube(0.05, 0.05, 0.12, 10, 1.0, 0.30f, 0.30f, 0.30f);
   glPushMatrix();
   glTranslated(0, 0.12, 0);
   glRotated(sprinklerAng, 0, 1, 0);
   Box(0, 0.03, 0, 0.14, 0.06, 0.05, 0, 0.20f, 0.20f, 0.22f);
   /* Only draw water arc when sprinkler is on AND not in Dry mode  */
   if (sprinklerOn && terrainMode != 0)
      SprinklerArc(0, 0.16, 0, 0);
   glPopMatrix();
}




/* 
 *  Grass clippings thrown from the mower deck.
 *  CPU stores : spawn position, initial velocity, birth time, maxLife.
 *  GPU computes: pos = SpawnPos + Velocity*age + 0.5*gravity*age^2
 *  in particle.vert -- no per-frame CPU position updates at all.
 *  
 */
static void SpawnParticles(double dt)
{
   /* Only spawn when shader is active -- particles need it to render */
   if (fabs(mowerSpd) < 0.01 || !useShader) return;
   int toSpawn = (int)(fabs(mowerSpd) * 80 * dt);
   for (int i = 0; i < toSpawn && numParticles < MAX_PARTS; i++)
   {
      GPUParticle* p = &particles[numParticles++];
      double ang = mowerAng * 3.14159265/180.0;
      p->x  = (float)(mowerX + 0.6*cos(ang) + 0.3*sin(ang));
      p->y  = (float)(TerrainHeight(mowerX, mowerZ) + 0.15);
      p->z  = (float)(mowerZ - 0.6*sin(ang) + 0.3*cos(ang));
      float rx = (float)(rand()%200-100)/100.0f;
      float rz = (float)(rand()%200-100)/100.0f;
      p->vx = (float)(sin(ang)*1.5 + rx*0.5);
      p->vy = 1.5f + (float)(rand()%100)/100.0f;
      p->vz = (float)(cos(ang)*1.5 + rz*0.5);
      p->birthTime = particleTime;
      p->maxLife   = 0.8f + (float)(rand()%60)/100.0f;
   }
}

static void UpdateParticles(double dt)
{
   particleTime += (float)dt;
   int alive = 0;
   for (int i = 0; i < numParticles; i++)
   {
      float age = particleTime - particles[i].birthTime;
      if (age < particles[i].maxLife)
         particles[alive++] = particles[i];
   }
   numParticles = alive;
}
/*  GPU computes position via particle.vert vertex shader  */
static void DrawParticles(void)
{
   if (numParticles == 0 || !shader_particle || !useShader) return;
   glUseProgram(shader_particle);
   /* Pass current time and terrain mode to shader */
   glUniform1f(glGetUniformLocation(shader_particle, "Time"),        particleTime);
   glUniform1i(glGetUniformLocation(shader_particle, "terrainMode"), terrainMode);
   /* Attribute locations for per-particle spawn data */
   int aSpawn = glGetAttribLocation(shader_particle, "SpawnPos");
   int aVel   = glGetAttribLocation(shader_particle, "Velocity");
   int aBirth = glGetAttribLocation(shader_particle, "BirthTime");
   int aMaxL  = glGetAttribLocation(shader_particle, "MaxLife");
   glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_2D);
   glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
   glBegin(GL_POINTS);
   for (int i = 0; i < numParticles; i++)
   {
      /* Upload spawn data -- GPU computes position using kinematic equation */
      glVertexAttrib1f(aBirth, particles[i].birthTime);
      glVertexAttrib1f(aMaxL,  particles[i].maxLife);
      glVertexAttrib3f(aVel,   particles[i].vx, particles[i].vy, particles[i].vz);
      glVertexAttrib3f(aSpawn, particles[i].x,  particles[i].y,  particles[i].z);
      glVertex3f(particles[i].x, particles[i].y, particles[i].z);
   }
   glEnd();
   glDisable(GL_BLEND); glDisable(GL_VERTEX_PROGRAM_POINT_SIZE);
   glUseProgram(0);
   if (light) glEnable(GL_LIGHTING);
}


/* +++ ADDED FOR FINAL: GPU rain system +++
 *  Raindrops stored as spawn positions + phase offsets.  The vertex
 *  shader rain.vert computes each drop's current y position using a
 *  repeating fall equation. 
 *  Activates automatically in Wet terrain mode or via 'y' key.
 */
static void InitRain(void)
{
   if (rainInited) return;
   for (int i = 0; i < MAX_RAIN; i++)
   {
      rainDrops[i].x     = (float)((rand()%1000)/1000.0 * 2*RAIN_FIELD - RAIN_FIELD);
      rainDrops[i].y     = 5.0f;  /* top of rain field */
      rainDrops[i].z     = (float)((rand()%1000)/1000.0 * 2*RAIN_FIELD - RAIN_FIELD);
      rainDrops[i].phase = (float)(rand()%1000)/1000.0f;
   }
   rainInited = 1;
}
static void DrawRain(void)
{
   /* Auto-activate in Wet mode, also caters manual toggle  [*/
   int shouldRain = rainOn || (terrainMode == 2);
   if (!shouldRain || !shader_rain) return;
   InitRain();
   glUseProgram(shader_rain);
   glUniform1f(glGetUniformLocation(shader_rain, "Time"),        particleTime);
   glUniform1f(glGetUniformLocation(shader_rain, "FallSpeed"),   0.6f);
   glUniform1f(glGetUniformLocation(shader_rain, "FieldHeight"), 5.5f);
   int aSpawn = glGetAttribLocation(shader_rain, "SpawnPos");
   int aPhase = glGetAttribLocation(shader_rain, "Phase");
   glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_2D);
   glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
   glBegin(GL_POINTS);
   for (int i = 0; i < MAX_RAIN; i++)
   {
      glVertexAttrib1f(aPhase, rainDrops[i].phase);
      glVertexAttrib3f(aSpawn, rainDrops[i].x, rainDrops[i].y, rainDrops[i].z);
      glVertex3f(rainDrops[i].x, rainDrops[i].y, rainDrops[i].z);
   }
   glEnd();
   glDisable(GL_BLEND); glDisable(GL_VERTEX_PROGRAM_POINT_SIZE);
   glUseProgram(0);
   if (light) glEnable(GL_LIGHTING);
}


/* =================================================================
 *  SUN / LIGHTING  [hw3, extended for day/night + headlight]
 * ================================================================= */

/*  Billboard sun disc with rays   */
static void DrawSun(void)
{
   int slices=32; float radius=0.4f, rayLen=0.5f; int rays=12;
   glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_2D);
   glColor3f(1.0f, 0.95f, 0.3f);
   glBegin(GL_TRIANGLE_FAN); glVertex3f(0, 0, 0);
   for (int i = 0; i <= slices; i++)
   { float a=360.0f*i/slices; glVertex3f(radius*Cos(a), radius*Sin(a), 0); }
   glEnd();
   glColor3f(1.0f, 0.75f, 0.0f);
   for (int i = 0; i < rays; i++)
   {
      float a=360.0f*i/rays, a1=a-7, a2=a+7;
      glBegin(GL_TRIANGLES);
      glVertex3f(radius*Cos(a1), radius*Sin(a1), 0);
      glVertex3f(radius*Cos(a2), radius*Sin(a2), 0);
      glVertex3f(rayLen*Cos(a),  rayLen*Sin(a),  0);
      glEnd();
   }
}

/*  Lighting: sun (GL_LIGHT0) + mower headlight spotlight (GL_LIGHT1)
 *  The headlight activates at night (sin(zh) < 0.15) so it is only
 *  visible during the night portion of the day/night cycle.   */
static void Lighting(double Lx, double Ly, double Lz)
{
   float day   = (float)Sin(zh);
   float above = day > 0 ? day : 0;
   /* Time-of-day diffuse: warm orange at dawn/dusk, white at noon  [hw3] */
   float Diffuse[] = {above*0.9f, above*0.85f, above*0.75f, 1};
   float tint = (above > 0 && above < 0.35f) ? (0.35f-above)/0.35f : 0;
   Diffuse[0] += tint*0.5f;
   Diffuse[2] -= tint*0.45f;
   if (Diffuse[2] < 0) Diffuse[2] = 0;
   float Ambient[] = {
      above>0 ? 0.08f+above*0.22f : 0.04f,
      above>0 ? 0.08f+above*0.22f : 0.04f,
      above>0 ? 0.08f+above*0.18f : 0.12f, 1};
   float s = above*0.35f;
   float Specular[] = {s, s, s, 1};
   float spec[]     = {0.3f, 0.3f, 0.3f, 1};
   /* Draw the sun billboard  [hw3] */
   if (Ly > 0)
   {
      glPushMatrix(); glTranslated(Lx, Ly, Lz); glRotated(zh, 0,0,1);
      DrawSun(); glPopMatrix();
   }
   /* GL_LIGHT0 -- sun  */
   float Position[] = {(float)Lx, (float)Ly, (float)Lz, 1};
   glEnable(GL_NORMALIZE); glEnable(GL_LIGHTING);
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
   glEnable(GL_COLOR_MATERIAL); glEnable(GL_LIGHT0);
   glMaterialf (GL_FRONT_AND_BACK, GL_SHININESS, 16);
   glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spec);
   glLightfv(GL_LIGHT0, GL_AMBIENT,  Ambient);
   glLightfv(GL_LIGHT0, GL_DIFFUSE,  Diffuse);
   glLightfv(GL_LIGHT0, GL_SPECULAR, Specular);
   glLightfv(GL_LIGHT0, GL_POSITION, Position);
   /* GL_LIGHT1 -- mower headlight spotlight (active at night)  */
   if (above < 0.15f)
   {
      double h   = TerrainHeight(mowerX, mowerZ);
      double ang = mowerAng * 3.14159265/180.0;
      float hlPos[] = {(float)(mowerX+0.5*sin(ang)),
                       (float)(h+0.45),
                       (float)(mowerZ+0.5*cos(ang)), 1};
      float hlDir[] = {(float)sin(ang), -0.15f, (float)cos(ang)};
      float hlDiff[] = {1, 0.95f, 0.7f, 1};
      float hlAmb[]  = {0.05f, 0.05f, 0.04f, 1};
      glEnable(GL_LIGHT1);
      glLightfv(GL_LIGHT1, GL_POSITION,       hlPos);
      glLightfv(GL_LIGHT1, GL_SPOT_DIRECTION, hlDir);
      glLightf (GL_LIGHT1, GL_SPOT_CUTOFF,    35);
      glLightf (GL_LIGHT1, GL_SPOT_EXPONENT,  15);
      glLightfv(GL_LIGHT1, GL_DIFFUSE,        hlDiff);
      glLightfv(GL_LIGHT1, GL_AMBIENT,        hlAmb);
      glLightfv(GL_LIGHT1, GL_SPECULAR,       hlDiff);
   }
   else glDisable(GL_LIGHT1);
}

/*  Sky colour derived from sun angle for smooth day/night transitions   */
static void SetSkyColor(void)
{
   float day = (float)Sin(zh), above = day>0 ? day : 0;
   float r, g, b;
   if (above > 0.3f)
      { r=0.45f*above; g=0.65f*above; b=0.90f*above; }
   else if (above > 0)
      { float t=above/0.3f; r=0.6f-0.2f*t; g=0.3f+0.2f*t; b=0.3f+0.4f*t; }
   else
      { r=0.02f; g=0.02f; b=0.08f; }
   glClearColor(r, g, b, 1);
}

/* 
 *  fogMode cycles GL_EXP -> GL_EXP2 -> GL_LINEAR with the ';' key so
 *  all three fog modes covered in the course can be compared directly.
 *  GL_FOG_HINT lets the driver choose per-vertex/per-fragment fog.
 *  glClearColor is matched to the fog colour so distant geometry fades
 *  into the background instead of into a mismatched sky colour.
 */
static void SetupFog(void)
{
   if (fogOn)
   {
      float day=(float)Sin(zh), above=day>0?day:0;
      float fogCol[] = {0.45f*above, 0.55f*above, 0.70f*above, 1};
      if (above <= 0) { fogCol[0]=0.02f; fogCol[1]=0.02f; fogCol[2]=0.05f; }
      GLuint fogModes[] = {GL_EXP, GL_EXP2, GL_LINEAR};
      glEnable(GL_FOG);
      glFogi(GL_FOG_MODE,    fogModes[fogMode]);
      glFogf(GL_FOG_DENSITY, 0.05f);
      glFogf(GL_FOG_START,   4.0f);
      glFogf(GL_FOG_END,    16.0f);
      glHint(GL_FOG_HINT,   GL_DONT_CARE);
      glFogfv(GL_FOG_COLOR, fogCol);
      glClearColor(fogCol[0], fogCol[1], fogCol[2], 1);
   }
   else glDisable(GL_FOG);
}


/* =================================================================
 *  SCENE ASSEMBLY  
 * ================================================================= */
static void Scene(void)
{
   /* Activate terrain shader if enabled  */
   if (useShader && shader_terrain)
   {
      glUseProgram(shader_terrain);
      glUniform1i(glGetUniformLocation(shader_terrain, "tex"),         0);
      glUniform1i(glGetUniformLocation(shader_terrain, "terrainMode"), terrainMode);
      glUniform1f(glGetUniformLocation(shader_terrain, "SunHeight"),   (float)Sin(zh));
   }
   DrawTerrain();
   if (useShader && shader_terrain) glUseProgram(0);

   /* Fence (display list, posts at terrain height) */
   glCallList(dl_fence);
   /* House */
   glPushMatrix(); glTranslated(-5.5, 0, -5.5); glRotated(45, 0,1,0);
   House(); glPopMatrix();
   /* Shed  */
   glPushMatrix(); glTranslated(6.0, 0, -5.5); glRotated(-30, 0,1,0);
   Shed(); glPopMatrix();
   /* Four trees  */
   glPushMatrix(); glTranslated(-6, 0, 4);  Tree(2.0, 1.2, 1.8); glPopMatrix();
   glPushMatrix(); glTranslated( 6, 0, 5);  Tree(2.5, 1.5, 2.0); glPopMatrix();
   glPushMatrix(); glTranslated(-3, 0,-7);  Tree(1.8, 1.0, 1.5); glPopMatrix();
   glPushMatrix(); glTranslated( 4, 0,-7);  Tree(2.2, 1.3, 1.7); glPopMatrix();
   /* Hedges  */
   glPushMatrix(); glTranslated(-4, 0, 0); glRotated(90, 0,1,0);
   HedgeSection(3.0); glPopMatrix();
   glPushMatrix(); glTranslated(5, 0, 2);
   HedgeSection(2.5); glPopMatrix();
   /* Garden beds */
   glPushMatrix(); glTranslated(-2, 0,-5); GardenBed(2.0, 0.8); glPopMatrix();
   glPushMatrix(); glTranslated( 1, 0,-5); GardenBed(1.8, 0.6); glPopMatrix();
   /* Gravel paths */
   GravelPath(-5.5,-3.5, 0, 0, 0.8);
   GravelPath(0, 0, 6,-3.5, 0.8);

   /* Waving flag near the house */
   {
      double t = glutGet(GLUT_ELAPSED_TIME) / 1000.0;
      glPushMatrix(); glTranslated(-3.0, 0, -3.0);
      FlagPole(t); glPopMatrix();
   }
   /* Sprinkler (arc only when on+non-dry) */
   glPushMatrix(); glTranslated(0.5, 0, -5.6);
   Sprinkler(); glPopMatrix();
  
   /* Dustbins */
   glPushMatrix(); glTranslated(4.5, 0, 3.5);
   Dustbin(0.30f, 0.55f, 0.30f); glPopMatrix();
   glPushMatrix(); glTranslated(5.5, 0, 3.2); glScaled(1.25, 1.25, 1.25);
   Dustbin(0.25f, 0.35f, 0.65f); glPopMatrix();
   /* Crates */
   Texture(tex_wood);
   Box(-7.0, 0.45, 5.0, 0.9,0.9,0.9,  20, 0.72f, 0.52f, 0.30f);
   Box(-6.2, 0.35, 5.5, 0.7,0.7,0.7, -15, 0.78f, 0.56f, 0.34f);
   /* Drivable mower  */
   DrawMower();
   /* grass-clipping particles  */
   if (useParticles) DrawParticles();
   /* Rain -- auto in Wet mode or 'y' key toggle  */
   DrawRain();
}

/*  Mouse object selection 
 *  Technique from course example ex31 (Asteroids):
 *  each pickable object is drawn with its Inspector index stored in
 *  the alpha channel via glColor4ub(..., id).  After the pick pass
 *  glReadPixels reads the pixel under the cursor; the alpha value
 *  gives the object ID, which is then mapped to the Inspector index
 *  so the user can click an object to inspect it directly.
 *
 *  Per-object hitbox sizes (w,h,d) and rotations (rotDeg) match each
 *  object's real footprint so elongated/thin objects (fence, hedge)
 *  are reliably clickable.  A polygon-offset bias prevents z-fighting
 *  between the pick boxes and the real geometry at the same depth.
 */
#define NUM_PICK_OBJS 10
typedef struct {
   double x, y, z;    /* World position of pick box centre */
   double w, h, d;    /* Full extents (width, height, depth) */
   double rotDeg;     /* Y-rotation matching the real object */
   int    inspIndex;  /* Inspector index this object maps to */
} PickEntry;

static PickEntry pickTable[NUM_PICK_OBJS] = {
   {  0.0, 0,  2.0,  1.3,1.0,1.0,   0, 0 }, /* Mower (live pos, updated each frame) */
   {  4.5, 0,  3.5,  0.9,1.0,0.9,   0, 1 }, /* Dustbin (green) */
   {  5.5, 0,  3.2,  1.1,1.2,1.1,   0, 2 }, /* Dustbin (blue, scaled larger) */
   { -5.5, 0, -5.5,  4.4,3.2,3.9,  45, 3 }, /* House -- rotated 45 deg */
   {  6.0, 0, -5.5,  2.3,2.1,2.0, -30, 4 }, /* Shed  -- rotated -30 deg */
   {  6.0, 0,  5.0,  2.6,2.0,2.6,   0, 5 }, /* Tree (large canopy) */
   {  9.0-0.3, 0, 0, 0.6,1.0,9.0,   0, 6 }, /* Fence (east rail, full length) */
   { -4.0, 0,  0,    0.7,1.0,3.0,  90, 7 }, /* Hedge (elongated, rotated 90) */
   { -3.0, 0, -3.0,  0.6,2.6,0.6,   0, 8 }, /* Flag  */
   {  0.5, 0, -5.6,  0.7,0.5,0.7,   0, 9 }  /* Sprinkler */
};

static void DrawPickPass(void)
{
   glDisable(GL_LIGHTING);
   glDisable(GL_TEXTURE_2D);
   glEnable(GL_POLYGON_OFFSET_FILL);
   glPolygonOffset(-2, -2);
   /* Keep mower pick marker at its live position */
   pickTable[0].x = mowerX;
   pickTable[0].z = mowerZ;
   for (int i = 0; i < NUM_PICK_OBJS; i++)
   {
      double h = TerrainHeight(pickTable[i].x, pickTable[i].z);
      glPushMatrix();
      glTranslated(pickTable[i].x, h + pickTable[i].h/2, pickTable[i].z);
      glRotated(pickTable[i].rotDeg, 0, 1, 0);
      glColor4ub(255, 255, 255, (unsigned char)(pickTable[i].inspIndex+1));
      glPushMatrix();
      glScaled(pickTable[i].w, pickTable[i].h, pickTable[i].d);
      glBegin(GL_QUADS);
        glVertex3d(-0.5,-0.5,-0.5); glVertex3d(-0.5,-0.5,+0.5);
        glVertex3d(+0.5,-0.5,+0.5); glVertex3d(+0.5,-0.5,-0.5);
        glVertex3d(-0.5,+0.5,-0.5); glVertex3d(+0.5,+0.5,-0.5);
        glVertex3d(+0.5,+0.5,+0.5); glVertex3d(-0.5,+0.5,+0.5);
        glVertex3d(-0.5,-0.5,-0.5); glVertex3d(-0.5,+0.5,-0.5);
        glVertex3d(-0.5,+0.5,+0.5); glVertex3d(-0.5,-0.5,+0.5);
        glVertex3d(+0.5,-0.5,-0.5); glVertex3d(+0.5,-0.5,+0.5);
        glVertex3d(+0.5,+0.5,+0.5); glVertex3d(+0.5,+0.5,-0.5);
      glEnd();
      glPopMatrix();
      glPopMatrix();
   }
   glDisable(GL_POLYGON_OFFSET_FILL);
   if (light) glEnable(GL_LIGHTING);
}

/*  Sample a (2R+1)x(2R+1) box around the cursor for robustness with
 *  thin objects like the fence rail and hedge.*/
static void UpdateSelection(int mx, int my)
{
   const int R = 3;
   selObj = -1;
   for (int dy = -R; dy <= R && selObj < 0; dy++)
      for (int dx = -R; dx <= R && selObj < 0; dx++)
      {
         unsigned char mc[4];
         glReadPixels(mx+dx, my+dy, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, mc);
         int id = mc[3];
         if (id >= 1 && id <= NUM_PICK_OBJS) selObj = id-1;
      }
}


/* =================================================================
 *  INSPECTOR MODE  
 *  Isolates one object on a turntable with an orbiting light.
 * ================================================================= */
static const char* inspNames[] = {
   "Riding Mower","Dustbin (green)","Dustbin (blue)","House",
   "Shed","Tree (large)","Fence Post","Hedge",
   "Flag","Sprinkler"
};

static void DrawInspectorObject(int idx)
{
   switch(idx)
   {
      case 0: { double sx=mowerX, sz=mowerZ, sa=mowerAng;
                mowerX=0; mowerZ=0; mowerAng=0; steerAng=0;
                DrawMower();
                mowerX=sx; mowerZ=sz; mowerAng=sa; } break;
      case 1: Dustbin(0.30f, 0.55f, 0.30f); break;
      case 2: glScaled(1.25,1.25,1.25); Dustbin(0.25f,0.35f,0.65f); break;
      case 3: House(); break;
      case 4: Shed(); break;
      case 5: Tree(2.5, 1.5, 2.0); break;
      case 6: Texture(tex_wood);
              Box(0,0.45,0, 0.12,0.9,0.12, 0, 0.66f,0.50f,0.32f); break;
      case 7: HedgeSection(2.0); break;
      case 8: { double t = glutGet(GLUT_ELAPSED_TIME)/1000.0;
                FlagPole(t); } break;
      case 9: Sprinkler(); break;
   }
}

/*  Per-object camera distance so every object fills the inspector frame */
static double InspectorCamDist(int idx)
{
   switch(idx)
   {
      case 0: return 5.0;    /* Mower */
      case 1: return 3.0;    /* Dustbin (green) */
      case 2: return 3.5;    /* Dustbin (blue)  */
      case 3: return 12.0;   /* House */
      case 4: return 7.0;    /* Shed */
      case 5: return 9.0;    /* Tree */
      case 6: return 2.5;    /* Fence post */
      case 7: return 5.5;    /* Hedge */
      case 8: return 6.0;    /* Flag */
      case 9: return 2.5;    /* Sprinkler */
      default: return 6.0;
   }
}

/*  Object-specific normal indicators using the exact same normal formula
 *  as the drawing code -- not hardcoded guesses.
 *  Drawing Lines and Anti-aliasing  */
static void DrawNormalIndicators(int idx)
{
   glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_2D);
   glColor3f(1, 0.2f, 0.2f);
   glEnable(GL_LINE_SMOOTH); glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
   glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glLineWidth(1.5f);
   glBegin(GL_LINES);
   if (idx == 1 || idx == 2)
   {
      /* Dustbin: outward cone normal N=(h*c, rb-rt, h*s) from Tube()  */
      double rb=0.30, rt=0.36, h=0.80;
      for (int i = 0; i <= 16; i++)
      {
         double a=360.0*i/16, c=Cos(a), s=Sin(a);
         double nx=h*c, ny=rb-rt, nz=h*s;
         double len=sqrt(nx*nx+ny*ny+nz*nz);
         nx/=len; ny/=len; nz/=len;
         double px=(rb+rt)*0.5*c, pz=(rb+rt)*0.5*s, py=0.4;
         glVertex3d(px, py, pz);
         glVertex3d(px+nx*0.3, py+ny*0.3, pz+nz*0.3);
      }
      /* Top/bottom cap normals (0,1,0) and (0,-1,0) from Tube() */
      glVertex3d(0, 0.80, 0); glVertex3d(0, 1.10, 0);
      glVertex3d(0, 0,    0); glVertex3d(0,-0.30, 0);
   }
   else if (idx == 5)
   {
      /* Tree canopy: same cone normal formula as Tube()  */
      double rb=1.5, rt=0.05, ch=1.1;
      for (int i = 0; i <= 12; i++)
      {
         double a=360.0*i/12, c=Cos(a), s=Sin(a);
         double nx=ch*c, ny=rb-rt, nz=ch*s;
         double len=sqrt(nx*nx+ny*ny+nz*nz);
         nx/=len; ny/=len; nz/=len;
         glVertex3d(rb*c, 2.3, rb*s);
         glVertex3d(rb*c+nx*0.4, 2.3+ny*0.4, rb*s+nz*0.4);
      }
   }
   else if (idx == 3)
   {
      /* House: normals taken from CubeTex() and House() glNormal3f calls  */
      glVertex3d( 0,1.5, 1.76); glVertex3d( 0,  1.5, 2.06); /* front  +Z */
      glVertex3d( 0,1.5,-1.76); glVertex3d( 0,  1.5,-2.06); /* back   -Z */
      glVertex3d( 2.1,1.5, 0);  glVertex3d( 2.4, 1.5, 0);   /* right  +X */
      glVertex3d(-2.1,1.5, 0);  glVertex3d(-2.4, 1.5, 0);   /* left   -X */
      glVertex3d( 0,3.6, 0.8);  glVertex3d( 0, 3.81, 1.01); /* roof front */
      glVertex3d( 0,3.6,-0.8);  glVertex3d( 0, 3.81,-1.01); /* roof back  */
   }
   else if (idx == 6)
   {
      /* Fence post: Box(0,0.45,0, 0.12,0.9,0.12) half-extents x=z=0.06 */
      glVertex3d( 0.06,0.45, 0);   glVertex3d( 0.36,0.45, 0);
      glVertex3d(-0.06,0.45, 0);   glVertex3d(-0.36,0.45, 0);
      glVertex3d( 0,   0.45, 0.06);glVertex3d( 0,   0.45, 0.36);
      glVertex3d( 0,   0.45,-0.06);glVertex3d( 0,   0.45,-0.36);
      glVertex3d( 0,   0.90, 0);   glVertex3d( 0,   1.20, 0);
   }
   else
   {
      /* Generic box: six face normals, starting from the face surface */
      double e = 0.7;
      glVertex3d( 0, e,  e); glVertex3d( 0,   e, e+0.4);
      glVertex3d( 0, e, -e); glVertex3d( 0,   e,-e-0.4);
      glVertex3d( e, e,  0); glVertex3d( e+0.4,e, 0);
      glVertex3d(-e, e,  0); glVertex3d(-e-0.4,e, 0);
      glVertex3d( 0,2*e,  0);glVertex3d( 0, 2*e+0.4, 0);
      glVertex3d( 0, 0,   0);glVertex3d( 0,  -0.4,   0);
   }
   glEnd();
   glDisable(GL_LINE_SMOOTH); glDisable(GL_BLEND); glLineWidth(1);
   if (light) glEnable(GL_LIGHTING);
}

static void DrawInspector(void)
{
   glClearColor(0.15f, 0.15f, 0.18f, 1);
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   glEnable(GL_DEPTH_TEST);
   glLoadIdentity();
   /* Per-object camera distance so every object fills the frame  */
   double cd = InspectorCamDist(inspObj);
   gluLookAt(cd*Sin(30)*Cos(20), cd*Sin(20)+1, cd*Cos(30)*Cos(20),
             0, 1, 0, 0, 1, 0);
   /* Orbiting light -- radius scales with camera distance  */
   float lr = (float)(cd * 0.65);
   float iPos[] = {lr*(float)Cos(inspAngle), (float)(cd*0.35),
                   lr*(float)Sin(inspAngle), 1};
   float iDiff[] = {0.8f, 0.8f, 0.8f, 1};
   float iAmb[]  = {0.25f, 0.25f, 0.25f, 1};
   float spec[]  = {0.3f, 0.3f, 0.3f, 1};
   glEnable(GL_NORMALIZE); glEnable(GL_LIGHTING);
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
   glEnable(GL_COLOR_MATERIAL); glEnable(GL_LIGHT0);
   glMaterialf (GL_FRONT_AND_BACK, GL_SHININESS, 16);
   glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spec);
   glLightfv(GL_LIGHT0, GL_POSITION, iPos);
   glLightfv(GL_LIGHT0, GL_DIFFUSE,  iDiff);
   glLightfv(GL_LIGHT0, GL_AMBIENT,  iAmb);
   /* Light position marker  [new] */
   glDisable(GL_LIGHTING); glColor3f(1, 1, 0.5f);
   glPushMatrix(); glTranslated(iPos[0], iPos[1], iPos[2]);
   Box(0,0,0, 0.15,0.15,0.15, 0, 1,1,0.5f); glPopMatrix();
   glEnable(GL_LIGHTING);
   /* Turntable  [new] */
   glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
   glPushMatrix(); glRotated(inspAngle*0.3, 0,1,0);
   DrawInspectorObject(inspObj);
   if (showNormals) DrawNormalIndicators(inspObj);
   glPopMatrix();
   /* HUD */
   glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_2D); glColor3f(1,1,1);
   glWindowPos2i(5, 45);
   Print("INSPECTOR: %s (%d/%d)  [n/N=cycle  v=normals  i=exit]",
         inspNames[inspObj], inspObj+1, NUM_INSP_OBJS);
   glWindowPos2i(5, 25);
   Print("Light angle=%.0f  Normals=%s", inspAngle, showNormals?"ON":"OFF");
}

/*  Help overlay Panel
 *  Two-column panel listing all key bindings, toggled with 'h'.
 *  Drawn in 2D screen space using glOrtho so it sits cleanly over
 *  the 3D scene regardless of camera mode.
 */
static void DrawHelp(void)
{
   int winW = glutGet(GLUT_WINDOW_WIDTH);
   int winH = glutGet(GLUT_WINDOW_HEIGHT);
   glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
   glOrtho(0, winW, 0, winH, -1, 1);
   glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
   glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_2D); glDisable(GL_DEPTH_TEST);
   glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   double pw=780, ph2=540;
   if (pw  > winW-40) pw  = winW-40;
   if (ph2 > winH-40) ph2 = winH-40;
   double px=(winW-pw)/2, py=(winH-ph2)/2;
   /* Background panel */
   glColor4f(0.04f, 0.04f, 0.07f, 0.90f);
   glBegin(GL_QUADS);
   glVertex2d(px,py); glVertex2d(px+pw,py);
   glVertex2d(px+pw,py+ph2); glVertex2d(px,py+ph2); glEnd();
   /* Border */
   glColor4f(0.85f,0.85f,0.90f,0.95f); glLineWidth(2.0f);
   glBegin(GL_LINE_LOOP);
   glVertex2d(px,py); glVertex2d(px+pw,py);
   glVertex2d(px+pw,py+ph2); glVertex2d(px,py+ph2); glEnd();
   glLineWidth(1.0f);
   /* Title bar */
   glColor4f(0.20f,0.45f,0.25f,0.55f);
   glBegin(GL_QUADS);
   glVertex2d(px,py+ph2-34); glVertex2d(px+pw,py+ph2-34);
   glVertex2d(px+pw,py+ph2); glVertex2d(px,py+ph2); glEnd();
   glDisable(GL_BLEND);
   /* Title text */
   glColor3f(1,1,1);
   glWindowPos2i((int)px+(int)pw/2-150, (int)(py+ph2)-24);
   Print("GROUNDSKEEPER  --  KEY BINDINGS");
   glColor3f(0.75f,0.85f,0.75f);
   glWindowPos2i((int)px+(int)pw/2-90, (int)(py+ph2)-40);
   Print("(press h to close this panel)");
   /* Two-column content */
   typedef struct { const char* header; const char* rows[4]; int n; } HelpSection;
   HelpSection left[] = {
      { "VIEW MODES", {
          "1 = Overhead Ortho      4 = Follow Mower",
          "2 = Overhead Persp      5 = Mower Seat",
          "3 = First-Person Walk   m = Cycle modes" }, 3 },
      { "DRIVING  (modes 4-5)", {
          "w / s = accelerate / brake",
          "a / d = steer left / right" }, 2 },
      { "WALKING  (mode 3)", {
          "w a s d = move",
          "arrow keys = look around" }, 2 },
      { "OVERHEAD  (modes 1-2)", {
          "arrow keys = orbit camera",
          "PgUp / PgDn = zoom in / out" }, 2 },
      { "OBJECT SELECTION", {
          "click  = select an object in the scene",
          "i      = jump into Inspector on selection" }, 2 }
   };
   HelpSection right[] = {
      { "SUN / ENVIRONMENT", {
          "space  = pause / resume sun motion",
          ", .    = sweep sun manually",
          "[ ]    = raise / lower sun",
          "l t f  = lighting / textures / fog" }, 4 },
      { "SHADER / TERRAIN / SPRINKLER", {
          "g  = toggle GLSL per-pixel shader",
          "p  = toggle GPU particles",
          "r  = cycle terrain (Dry/Moderate/Wet)",
          "y  = toggle rain (auto in Wet mode)" }, 4 },
      { "INSPECTOR MODE", {
          "i      = enter / exit Inspector",
          "n N    = next / previous object",
          "v      = toggle surface normals" }, 3 },
      { "GENERAL", {
          "b      = toggle axes",
          "+ / -  = field of view",
          "0      = reset everything",
          "ESC q  = quit" }, 4 }
   };
   int rowH=20, headerGap=28;
   int colX[2] = { (int)px+28, (int)px+(int)pw/2+18 };
   HelpSection* cols[2] = { left, right };
   int counts[2] = { (int)(sizeof(left)/sizeof(left[0])),
                     (int)(sizeof(right)/sizeof(right[0])) };
   for (int c = 0; c < 2; c++)
   {
      int y = (int)(py+ph2) - 70;
      for (int s = 0; s < counts[c]; s++)
      {
         glColor3f(0.55f, 0.95f, 0.55f);
         glWindowPos2i(colX[c], y);
         Print("%s", cols[c][s].header);
         glColor3f(0.35f, 0.55f, 0.35f);
         glBegin(GL_LINES);
         glVertex2i(colX[c], y-6); glVertex2i(colX[c]+300, y-6);
         glEnd();
         y -= headerGap;
         glColor3f(0.92f, 0.92f, 0.92f);
         for (int r = 0; r < cols[c][s].n; r++)
         {
            glWindowPos2i(colX[c]+8, y);
            Print("%s", cols[c][s].rows[r]);
            y -= rowH;
         }
         y -= 14;
      }
   }
   glEnable(GL_DEPTH_TEST);
   glMatrixMode(GL_PROJECTION); glPopMatrix();
   glMatrixMode(GL_MODELVIEW);  glPopMatrix();
}
/*  Help overlay Panel  */

/* =================================================================
 *  DISPLAY 
 * ================================================================= */
void display(void)
{
   if (inspector) {
      DrawInspector(); ErrCheck("display-inspector");
      glFlush(); glutSwapBuffers(); return;
   }
   /* SetupFog sets glClearColor when fog is on; SetSkyColor otherwise */
   if (!fogOn) SetSkyColor();
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   glEnable(GL_DEPTH_TEST);
   glLoadIdentity();
   /* Camera  [hw3 + new] */
   if (mode == 2) {
      double lx=Cos(fpPh)*Sin(fpTh), ly=Sin(fpPh), lz=-Cos(fpPh)*Cos(fpTh);
      gluLookAt(Ex,Ey,Ez, Ex+lx,Ey+ly,Ez+lz, 0,1,0);
   } else if (mode == 3) {
      double h=TerrainHeight(mowerX,mowerZ), ang=mowerAng*3.14159265/180.0;
      gluLookAt(mowerX-3*sin(ang), h+2.5, mowerZ-3*cos(ang),
                mowerX, h+0.5, mowerZ, 0,1,0);
   } else if (mode == 4) {
      double h=TerrainHeight(mowerX,mowerZ), ang=mowerAng*3.14159265/180.0;
      gluLookAt(mowerX-0.1*sin(ang), h+1.2, mowerZ-0.1*cos(ang),
                mowerX+3*sin(ang), h+0.8, mowerZ+3*cos(ang), 0,1,0);
   } else if (mode == 1) {
      double Px=-2*dim*Sin(th)*Cos(ph), Py=2*dim*Sin(ph), Pz=2*dim*Cos(th)*Cos(ph);
      gluLookAt(Px,Py,Pz, 0,0,0, 0,Cos(ph),0);
   } else { glRotatef(ph,1,0,0); glRotatef(th,0,1,0); }

   double Lx=LR*Cos(zh), Ly=LR*Sin(zh)+sunElev, Lz=0;
   if (light) Lighting(Lx, Ly, Lz); else glDisable(GL_LIGHTING);
   glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
   SetupFog();

   /*  Pick pass for mouse selection
    *  Render colour-ID boxes, read the pixel, then re-render the real
    *  scene.  Fog is disabled during the pick pass so the ID colours
    *  are not distorted by the fog blend.                 */
   glDisable(GL_FOG);
   DrawPickPass();
   glFlush();
   { int my_gl = glutGet(GLUT_WINDOW_HEIGHT) - pickMouseY;
     UpdateSelection(pickMouseX, my_gl); }
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   if (light) Lighting(Lx, Ly, Lz); else glDisable(GL_LIGHTING);
   SetupFog();


   Scene();

   /* Axes  */
   glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_2D);
   if (axes) {
      const double len=2;
      glColor3f(1,1,1); glBegin(GL_LINES);
      glVertex3d(0,0,0); glVertex3d(len,0,0);
      glVertex3d(0,0,0); glVertex3d(0,len,0);
      glVertex3d(0,0,0); glVertex3d(0,0,len); glEnd();
      glRasterPos3d(len,0,0); Print("X");
      glRasterPos3d(0,len,0); Print("Y");
      glRasterPos3d(0,0,len); Print("Z");
   }
   /* HUD */
   glColor3f(1,1,1);
   const char* mn[] = {"Overhead Ortho","Overhead Persp","First Person",
                       "Follow Mower","Mower Seat"};
   const char* tod   = (Ly>0) ? (Sin(zh)<0.35f?"Dawn/Dusk":"Day") : "Night";
   const char* tname[] = {"Dry","Moderate","Wet"};
   const char* fmname[]= {"EXP","EXP2","LINEAR"};
   glWindowPos2i(5, 85);
   Print("Mode=%s  %s  zh=%.0f  Fog=%s(%s)  Tex=%s",
         mn[mode], tod, zh,
         fogOn?"On":"Off", fmname[fogMode], ntex?"On":"Off");
   glWindowPos2i(5, 65);
   Print("Mower=(%.1f,%.1f) Hdg=%.0f Spd=%.1f  Shader=%s Terrain=%s Rain=%s",
         mowerX, mowerZ, mowerAng, mowerSpd,
         useShader?"On":"Off", tname[terrainMode],
         (rainOn||(terrainMode==2))?"On":"Off");
   glWindowPos2i(5, 45);
   if (selObj >= 0)
      Print("Selected: %s  (press i to inspect)", inspNames[selObj]);
   glWindowPos2i(5, 25);
   Print("[1-5]=Mode WASD=Drive r=Terrain g=Shader p=Particles k=Sprinkler i=Insp h=Help q=Quit");
   /* +++ ADDED FOR FINAL: help overlay +++ */
   if (helpOn) DrawHelp();
   ErrCheck("display");
   glFlush(); glutSwapBuffers();
}

/* Mower collision detection 
 *  Two obstacle shapes:
 *    Circles -- trees, dustbins, crates, flagpole, sprinkler
 *    Rotated boxes -- house and shed
 *  Collision response is a SLIDE: cancel only the penetrating component
 *  of the movement vector so the mower glances off smoothly.  [new]
 */
typedef struct { double x, z, radius; } CircleObs;
typedef struct { double x, z, halfW, halfD, rotDeg; } BoxObs;

static CircleObs circleObs[NUM_CIRCLE_OBS] = {
   { -6.0,  4.0, 1.10 }, /* Tree 1 */
   {  6.0,  5.0, 1.35 }, /* Tree 2 (largest) */
   { -3.0, -7.0, 0.95 }, /* Tree 3 */
   {  4.0, -7.0, 1.15 }, /* Tree 4 */
   {  4.5,  3.5, 0.45 }, /* Dustbin (green) */
   {  5.5,  3.2, 0.55 }, /* Dustbin (blue)  */
   { -7.0,  5.0, 0.55 }, /* Crate */
   { -3.0, -3.0, 0.10 }, /* Flagpole */
   {  0.5, -5.6, 0.10 }  /* Sprinkler */
};
static BoxObs boxObs[NUM_BOX_OBS] = {
   { -5.5, -5.5, 2.2, 1.9,  45 }, /* House */
   {  6.0, -5.5, 1.3, 1.1, -30 }  /* Shed  */
};

/*  Test mower against a rotated box obstacle.  Transforms the mower
 *  into the box's local frame for a simple AABB test, then rotates
 *  the outward normal back to world space for the sliding response. */
static int PointInBox(double x, double z, BoxObs b,
                      double *nx, double *nz)
{
   double rad = -b.rotDeg * 3.14159265/180.0;
   double dx = x-b.x, dz = z-b.z;
   double lx =  dx*cos(rad) - dz*sin(rad);
   double lz =  dx*sin(rad) + dz*cos(rad);
   double mw = b.halfW + MOWER_RADIUS;
   double md = b.halfD + MOWER_RADIUS;
   if (lx > -mw && lx < mw && lz > -md && lz < md)
   {
      /* Find nearest edge and return its outward normal in world space */
      double dL=lx+mw, dR=mw-lx, dB=lz+md, dF=md-lz;
      double minD=dL; double lnx=-1, lnz=0;
      if (dR<minD){minD=dR; lnx=1;  lnz=0; }
      if (dB<minD){minD=dB; lnx=0;  lnz=-1;}
      if (dF<minD){         lnx=0;  lnz=1; }
      double wrad = b.rotDeg * 3.14159265/180.0;
      *nx = lnx*cos(wrad) - lnz*sin(wrad);
      *nz = lnx*sin(wrad) + lnz*cos(wrad);
      return 1;
   }
   return 0;
}

/*  Slide the proposed displacement (dx,dz) along any obstacle it
 *  would penetrate.  [new] */
static void ResolveCollision(double curX, double curZ,
                             double *dx, double *dz)
{
   double newX = curX + *dx, newZ = curZ + *dz;
   /* Circular obstacles */
   for (int i = 0; i < NUM_CIRCLE_OBS; i++)
   {
      double ox=circleObs[i].x, oz=circleObs[i].z;
      double minDist=circleObs[i].radius+MOWER_RADIUS;
      double ddx=newX-ox, ddz=newZ-oz;
      double dist=sqrt(ddx*ddx+ddz*ddz);
      if (dist < minDist && dist > 1e-6)
      {
         double nx=ddx/dist, nz=ddz/dist;
         double into = -((*dx)*nx + (*dz)*nz);
         if (into > 0) { *dx += into*nx; *dz += into*nz; }
         newX=curX+*dx; newZ=curZ+*dz;
      }
   }
   /* Rectangular obstacles */
   for (int i = 0; i < NUM_BOX_OBS; i++)
   {
      double nx, nz;
      if (PointInBox(newX, newZ, boxObs[i], &nx, &nz))
      {
         double into = -((*dx)*nx + (*dz)*nz);
         if (into > 0) { *dx += into*nx; *dz += into*nz; }
         newX=curX+*dx; newZ=curZ+*dz;
      }
   }
}
/*  Mower collision detection  */

/* =================================================================
 *  CALLBACKS  
 * ================================================================= */
void idle(void)
{
   static double last = 0;
   double t  = glutGet(GLUT_ELAPSED_TIME)/1000.0;
   double dt = t - last;
   if (dt > 0.05) dt = 0.05;
   last = t;
   if (move) zh = fmod(zh + 36*dt, 360);
   /* Mower physics with collision detection  [new] */
   if (mode == 3 || mode == 4)
   {
      mowerSpd *= (1.0 - 2.0*dt);
      if (fabs(mowerSpd) < 0.01) mowerSpd = 0;
      mowerAng += steerAng*mowerSpd*dt*0.8;
      mowerAng  = fmod(mowerAng+360, 360);
      double rad = mowerAng * 3.14159265/180.0;
      /* Compute proposed displacement then slide along obstacles  [new] */
      double moveDx = sin(rad)*mowerSpd*dt;
      double moveDz = cos(rad)*mowerSpd*dt;
      ResolveCollision(mowerX, mowerZ, &moveDx, &moveDz);
      mowerX += moveDx;
      mowerZ += moveDz;
      if (mowerX >  FIELD-1){ mowerX =  FIELD-1; mowerSpd=0; }
      if (mowerX < -FIELD+1){ mowerX = -FIELD+1; mowerSpd=0; }
      if (mowerZ >  FIELD-1){ mowerZ =  FIELD-1; mowerSpd=0; }
      if (mowerZ < -FIELD+1){ mowerZ = -FIELD+1; mowerSpd=0; }
      bladeRot += 720*dt;
      if (fabs(mowerSpd) > 0.05) UpdateMowing();
   }
   if (inspector) inspAngle = fmod(inspAngle + 30*dt, 360);
   /* GPU particle birth/death  */
   if (useParticles) { SpawnParticles(dt); UpdateParticles(dt); }
   /* Sprinkler rotation + mist */
   sprinklerAng = fmod(sprinklerAng + 25*dt, 360);
  
   glutPostRedisplay();
}

static void SetProjection(void)
{
   if      (mode == 0) Project(0,   asp, dim);
   else if (mode == 1) Project(fov, asp, dim);
   else                Project(70,  asp, dim);
}

void special(int key, int x, int y)
{
   (void)x; (void)y;
   if (mode == 2) {
      if      (key==GLUT_KEY_RIGHT) fpTh+=4;
      else if (key==GLUT_KEY_LEFT)  fpTh-=4;
      else if (key==GLUT_KEY_UP)    fpPh+=3;
      else if (key==GLUT_KEY_DOWN)  fpPh-=3;
      if (fpPh >  85) fpPh =  85;
      if (fpPh < -85) fpPh = -85;
      fpTh = fmod(fpTh+360, 360);
   } else if (mode < 2) {
      if      (key==GLUT_KEY_RIGHT)     th+=5;
      else if (key==GLUT_KEY_LEFT)      th-=5;
      else if (key==GLUT_KEY_UP)        ph+=5;
      else if (key==GLUT_KEY_DOWN)      ph-=5;
      else if (key==GLUT_KEY_PAGE_UP)   dim+=0.5;
      else if (key==GLUT_KEY_PAGE_DOWN) dim-=0.5;
      th%=360; ph%=360;
      if (dim < 2) dim = 2;
   }
   SetProjection(); glutPostRedisplay();
}

void key(unsigned char ch, int x, int y)
{
   (void)x; (void)y;
   double step = 0.4;
   if (ch==27||ch=='q'||ch=='Q') exit(0);
   else if (ch == '0') {
      th=35; ph=30; dim=12; fov=55;
      Ex=0; Ey=1.3; Ez=7; fpTh=0; fpPh=-8;
      light=1; move=1; ntex=1; zh=90; sunElev=0; fogOn=0;
      useShader=1; useParticles=1; terrainMode=0; fogMode=1;
      memset(mowGrid, 0, sizeof(mowGrid));
      numParticles=0; particleTime=0;
      helpOn=0; selObj=-1; sprinklerAng=0; sprinklerOn=1;
      rainOn=0;
      mowerX=0; mowerZ=2; mowerAng=0; mowerSpd=0; steerAng=0;
      inspector=0; inspObj=0; showNormals=0;
   }
   else if (ch=='i'||ch=='I') {
      inspector = 1-inspector;
      /* Jump straight to the selected object when entering inspector  [new] */
      if (inspector && selObj >= 0) inspObj = selObj;
   }
   else if (ch=='n' && inspector) inspObj=(inspObj+1)%NUM_INSP_OBJS;
   else if (ch=='N' && inspector) inspObj=(inspObj+NUM_INSP_OBJS-1)%NUM_INSP_OBJS;
   else if ((ch=='v'||ch=='V') && inspector) showNormals=1-showNormals;
   else if (ch=='1') mode=0;
   else if (ch=='2') mode=1;
   else if (ch=='3') mode=2;
   else if (ch=='4') mode=3;
   else if (ch=='5') mode=4;
   else if (ch=='m'||ch=='M') mode=(mode+1)%5;
   else if (ch=='l'||ch=='L') light=1-light;
   else if (ch==' ')          move=1-move;
   else if (ch==',') zh=fmod(zh-5+360, 360);
   else if (ch=='.') zh=fmod(zh+5,     360);
   else if (ch=='[') sunElev-=0.5;
   else if (ch==']') sunElev+=0.5;
   else if (ch=='t'||ch=='T') ntex=1-ntex;
   else if (ch=='f'||ch=='F') fogOn=1-fogOn;
   else if (ch=='g'||ch=='G') useShader=1-useShader;
   else if (ch=='p'||ch=='P') useParticles=1-useParticles;
   else if (ch=='r'||ch=='R') terrainMode=(terrainMode+1)%3;
   else if (ch=='h'||ch=='H') helpOn=1-helpOn;
   else if (ch==';')          fogMode=(fogMode+1)%3;
   else if (ch=='k'||ch=='K') sprinklerOn=1-sprinklerOn; 
   else if (ch=='y'||ch=='Y') rainOn=1-rainOn;             
   else if (ch=='b'||ch=='B') axes=1-axes;
   else if (ch=='-' && fov>5)   fov--;
   else if (ch=='+' && fov<160) fov++;
   /* Driving  [new] */
   else if ((mode==3||mode==4)&&(ch=='w'||ch=='W')) mowerSpd+=0.8;
   else if ((mode==3||mode==4)&&(ch=='s'||ch=='S')) mowerSpd-=0.8;
   else if ((mode==3||mode==4)&&(ch=='a'||ch=='A'))
      steerAng = steerAng<30  ? steerAng+5 :  30;
   else if ((mode==3||mode==4)&&(ch=='d'||ch=='D'))
      steerAng = steerAng>-30 ? steerAng-5 : -30;
   /* Walking  [hw3] */
   else if (mode==2&&(ch=='w'||ch=='W')){ Ex+=step*Sin(fpTh); Ez-=step*Cos(fpTh); }
   else if (mode==2&&(ch=='s'||ch=='S')){ Ex-=step*Sin(fpTh); Ez+=step*Cos(fpTh); }
   else if (mode==2&&(ch=='d'||ch=='D')){ Ex+=step*Cos(fpTh); Ez+=step*Sin(fpTh); }
   else if (mode==2&&(ch=='a'||ch=='A')){ Ex-=step*Cos(fpTh); Ez-=step*Sin(fpTh); }
   /* Boundary clamps  [hw3] */
   if (Ex >  FIELD-0.5) Ex =  FIELD-0.5;
   if (Ex < -FIELD+0.5) Ex = -FIELD+0.5;
   if (Ez >  FIELD-0.5) Ez =  FIELD-0.5;
   if (Ez < -FIELD+0.5) Ez = -FIELD+0.5;
   if (mowerSpd >  6)  mowerSpd =  6;
   if (mowerSpd < -2)  mowerSpd = -2;
   if (mode==3||mode==4)
      if (ch!='a'&&ch!='A'&&ch!='d'&&ch!='D') steerAng*=0.7;
   SetProjection(); glutPostRedisplay();
}

/*  mouse callbacks for object selection 
 *  Passive/active motion tracks cursor for the pick pass.
 *  Left click while not in inspector jumps straight to Inspector
 *  mode on the selected object.  [ex31 mouse-tracking + new click logic]
 */
void motionFn(int x, int y)
{
   pickMouseX = x; pickMouseY = y;
   glutPostRedisplay();
}
void mouseFn(int button, int state, int x, int y)
{
   pickMouseX = x; pickMouseY = y;
   if (button==GLUT_LEFT_BUTTON && state==GLUT_DOWN && !inspector && selObj>=0)
   {
      inspector = 1;
      inspObj   = selObj;
   }
   glutPostRedisplay();
}


void reshape(int w, int h)
{
   asp = h>0 ? (double)w/h : 1;
   glViewport(0, 0, w, h);
   SetProjection();
}

/*  Compile fence into a display list so it is not re-submitted every
 *  frame. */
static void CompileDisplayLists(void)
{
   dl_fence = glGenLists(1);
   glNewList(dl_fence, GL_COMPILE);
   Fence();
   glEndList();
}

int main(int argc, char* argv[])
{
   srand((unsigned)time(NULL));
   memset(mowGrid, 0, sizeof(mowGrid));
   glutInit(&argc, argv);
   /* GLUT_ALPHA required so glReadPixels can retrieve the alpha channel
    * used to encode object IDs for mouse picking */
   glutInitDisplayMode(GLUT_RGB|GLUT_DEPTH|GLUT_DOUBLE|GLUT_ALPHA);
   glutInitWindowSize(900, 700);
   glutCreateWindow("Prithvikiran Premkumar - Groundskeeper");
#ifdef USEGLEW
   if (glewInit() != GLEW_OK) Fatal("Error initializing GLEW\n");
#endif
   glutDisplayFunc(display);
   glutReshapeFunc(reshape);
   glutSpecialFunc(special);
   glutKeyboardFunc(key);
   glutIdleFunc(idle);
   glutMouseFunc(mouseFn);
   glutMotionFunc(motionFn);
   glutPassiveMotionFunc(motionFn);
   /* Load textures   */
   tex_grass   = LoadTexBMP("Textures/grass.bmp");
   tex_wood    = LoadTexBMP("Textures/wood.bmp");
   tex_metal   = LoadTexBMP("Textures/metal.bmp");
   tex_plastic = LoadTexBMP("Textures/plastic.bmp");
   tex_rubber  = LoadTexBMP("Textures/rubber.bmp");
   tex_stone   = LoadTexBMP("Textures/stone.bmp");
   tex_roof    = LoadTexBMP("Textures/roof.bmp");
   tex_bark    = LoadTexBMP("Textures/bark.bmp");
   tex_leaf    = LoadTexBMP("Textures/leaf.bmp");
   tex_hedge   = LoadTexBMP("Textures/hedge.bmp");
   tex_gravel  = LoadTexBMP("Textures/gravel.bmp");
   tex_dirt    = LoadTexBMP("Textures/dirt.bmp");
   /*  compile GLSL shaders */
   shader_terrain  = CreateShaderProg("terrain.vert",  "terrain.frag");
   shader_particle = CreateShaderProg("particle.vert", "particle.frag");
   shader_flag     = CreateShaderProg("flag.vert",     "flag.frag");
   shader_rain     = CreateShaderProg("rain.vert",     "rain.frag");
   CompileDisplayLists();
   ErrCheck("init");
   glutMainLoop();
   return 0;
}
