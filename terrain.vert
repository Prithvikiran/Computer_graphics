#version 120
varying vec3  View;
varying vec3  Light;
varying vec3  Light2;    /* Mower headlight direction */
varying vec3  Normal;
varying vec3  WorldPos;  /* World position for mow grid */
varying float Dist;      /* Distance from camera */

uniform float SunHeight; 

void main()
{
   //  Vertex location in modelview coordinates 
   vec4 P = gl_ModelViewMatrix * gl_Vertex;

   //  Sun direction 
   Light  = gl_LightSource[0].position.xyz - P.xyz;

   //  Mower headlight direction 
   Light2 = gl_LightSource[1].position.xyz - P.xyz;

   //  Normal 
   Normal = gl_NormalMatrix * gl_Normal;

   //  Eye position 
   View = -P.xyz;

   //  World position for mow grid and slope checks 
   WorldPos = gl_Vertex.xyz;

   //  Distance from camera for distance darkening 
   Dist = length(P.xyz);

   //  Texture 
   gl_TexCoord[0] = gl_MultiTexCoord0;

   //  Screen position
   gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}