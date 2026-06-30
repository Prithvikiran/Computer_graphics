//  Flag vertex shader - Groundskeeper - Prithvikiran Premkumar - CSCI 5229
//  Per-pixel lighting for the waving flag cloth surface.
//  Passes sun direction, headlight direction, normal, view direction,
//  texture coordinates, and the wave displacement (WaveZ) to the
//  fragment shader so it can compute granular per-pixel shading
//  including wave-crest brightening and two-sided cloth lighting.
#version 120

varying vec3  Light;     /* Direction to sun (GL_LIGHT0)        */
varying vec3  Light2;    /* Direction to headlight (GL_LIGHT1)  */
varying vec3  Normal;    /* Surface normal in camera space       */
varying vec3  View;      /* Direction back to camera eye         */
varying vec2  TexCoord;  /* UV coordinates for flag zone mapping */
varying float WaveZ;     /* Wave displacement -- used for crest/trough tint */

void main()
{
   //  Vertex in camera space
   vec4 P = gl_ModelViewMatrix * gl_Vertex;

   //  Sun and headlight directions
   Light  = gl_LightSource[0].position.xyz - P.xyz;
   Light2 = gl_LightSource[1].position.xyz - P.xyz;

   //  Surface normal transformed into camera space
   Normal = gl_NormalMatrix * gl_Normal;

   //  Direction back to camera eye
   View = -P.xyz;

   //  Texture coordinates for flag zone colouring in fragment shader
   TexCoord = gl_MultiTexCoord0.st;

   //  Wave displacement in world space (gl_Vertex.z in flag local frame)
   //  Used by fragment shader to brighten crests and darken troughs
   WaveZ = gl_Vertex.z;

   gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}
