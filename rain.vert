
//  GPU rain particle system.  Each raindrop stores only its spawn
//  position and a random phase offset.  The GPU computes the current
//  position using a repeating fall equation so raindrops cycle
//  continuously .
//
//  Fall equation: y = SpawnY - fract((Time + Phase) * FallSpeed) * FieldHeight
//  When a drop reaches the ground it wraps back to the top automatically.
#version 120

attribute vec3  SpawnPos;  /* Initial spawn position (x, top_y, z) */
attribute float Phase;     /* Random phase offset so drops are staggered */

uniform float Time;        /* Elapsed time from C code */
uniform float FallSpeed;   /* How fast rain falls */
uniform float FieldHeight; /* Height range drops fall over */

varying float Alpha;       /* Fade value for fragment shader */

void main()
{
   //  Compute how far through its cycle this drop is (0=top, 1=bottom)
   float cycle = fract((Time * FallSpeed + Phase));

   //  Current position: x and z fixed, y falls from top to ground
   vec3 pos = vec3(SpawnPos.x,
                   SpawnPos.y - cycle * FieldHeight,
                   SpawnPos.z);

   //  Slight wind angle -- offset x as drop falls
   pos.x += cycle * 0.8;

   //  Fade in at top, fade out near ground
   Alpha = 1.0 - abs(cycle - 0.5) * 0.5;

   gl_PointSize = 2.5;
   gl_Position  = gl_ModelViewProjectionMatrix * vec4(pos, 1.0);
}
