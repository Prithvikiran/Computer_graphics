//  Particle fragment shader - Groundskeeper - Prithvikiran Premkumar - CSCI 5229
//  Colours particles based on terrain mode (dry/moderate/wet)
#version 120

varying float Alpha;

uniform int terrainMode; /* 0=Dry 1=Moderate 2=Wet */

void main()
{
   vec3 particleCol;

   if (terrainMode == 0)
      /* Dry -- sandy brown dust particles */
      particleCol = vec3(0.75, 0.60, 0.30);
   else if (terrainMode == 1)
      /* Moderate -- light green grass clippings */
      particleCol = vec3(0.55, 0.75, 0.30);
   else
      /* Wet -- dark green wet grass clippings */
      particleCol = vec3(0.20, 0.45, 0.15);

   gl_FragColor = vec4(particleCol, Alpha);
}