//  Particle vertex shader - Groundskeeper - Prithvikiran Premkumar - CSCI 5229
//  Computes particle position on GPU using p = p0 + v*t + 0.5*g*t^2

#version 120

attribute vec3  SpawnPos;  /* Where particle was born */
attribute vec3  Velocity;  /* Initial velocity */
attribute float BirthTime; /* When particle was spawned */
attribute float MaxLife;   /* Total lifespan */

uniform float Time;        /* Current elapsed time */

varying float Alpha;       /* Fade value passed to fragment shader */

void main()
{
   //  Age of this particle
   float age = Time - BirthTime;

   //  Gravity vector
   vec3 gravity = vec3(0.0, -4.0, 0.0);

   //  Kinematic equation: p = p0 + v*t + 0.5*g*t^2
   //  GPU computes position -- no CPU involvement
   vec3 pos = SpawnPos + Velocity*age + 0.5*gravity*age*age;

   //  Fade out as particle ages
   Alpha = 1.0 - (age / MaxLife);

   //  Set point size and position
   gl_PointSize = 4.0;
   gl_Position  = gl_ModelViewProjectionMatrix * vec4(pos, 1.0);
}