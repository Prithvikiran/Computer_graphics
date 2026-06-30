
#version 120

varying vec3  View;
varying vec3  Light;
varying vec3  Light2;    /* Mower headlight direction */
varying vec3  Normal;
varying vec3  WorldPos;  /* World space position */
varying float Dist;      /* Camera distance */

uniform sampler2D tex;
uniform int   terrainMode; /* 0=Spring 1=Summer 2=Rainy */
uniform float SunHeight;   /* Sun height for tint */

void main()
{
   //  N is the surface normal 
   vec3 N = normalize(Normal);

   //  L is the sun direction 
   vec3 L = normalize(Light);

   //  R is the reflected sun vector 
   vec3 R = reflect(-L, N);

   //  V is the view direction 
   vec3 V = normalize(View);

   //  Sun diffuse and specular 
   float Id = max(dot(L,N), 0.0);
   float Is = (Id>0.0) ? pow(max(dot(R,V),0.0), gl_FrontMaterial.shininess) : 0.0;

   //  Base lighting from sun 
   vec4 color = gl_FrontMaterial.emission
              + gl_FrontLightProduct[0].ambient
              + Id * gl_FrontLightProduct[0].diffuse
              + Is * gl_FrontLightProduct[0].specular;

   /* Mower headlight as second light source 
    *  Computes diffuse + specular from GL_LIGHT1 independently
    *  and adds it to the base colour. Active at night.       */
   vec3  L2  = normalize(Light2);
   vec3  R2  = reflect(-L2, N);
   float Id2 = max(dot(L2,N), 0.0);
   float Is2 = (Id2>0.0) ? pow(max(dot(R2,V),0.0), gl_FrontMaterial.shininess) : 0.0;
   color += Id2 * gl_FrontLightProduct[1].diffuse
          + Is2 * gl_FrontLightProduct[1].specular;

   /* Terrain colour: 3 seasonal modes toggled with 'r' key.
    *  0=Spring  1=Summer/Dry  2=Rainy
    *  Slope blends flat colour toward steep colour using N.y
    *  (1=flat ground, 0=vertical surface) [new]             */
   float slope = 1.0 - N.y;

   vec3 flatCol, steepCol;
  if (terrainMode == 0)
   {
      /* Dry -- parched yellow-brown grass */
      flatCol  = vec3(0.72, 0.65, 0.25);
      steepCol = vec3(0.55, 0.45, 0.18);
   }
   else if (terrainMode == 1)
   {
      /* Moderate -- healthy green grass */
      flatCol  = vec3(0.25, 0.58, 0.20);
      steepCol = vec3(0.18, 0.42, 0.15);
   }
   else
   {
      /* Wet/Rainy -- dark saturated green, muddy slopes */
      flatCol  = vec3(0.18, 0.48, 0.15);
      steepCol = vec3(0.35, 0.28, 0.15);
   }
   vec3 terrainCol = mix(flatCol, steepCol, smoothstep(0.2, 0.6, slope));

   /* Mowed vs unmowed colour shift [new]
    *  Mowed areas slightly lighter. Uses WorldPos to determine
    *  position in yard. Only applied in Spring mode.         */
   float mowBrightness = 1.0;
   float distFromCentre = length(WorldPos.xz);
   if (distFromCentre < 4.0 && terrainMode == 0)
      mowBrightness = 1.15;
   terrainCol *= mowBrightness;

   /* Sun height colour tint [new]
    *  Warm orange tint at sunrise/sunset (SunHeight near 0)
    *  Cool white at noon (SunHeight near 1)                  */
   float warmth = 1.0 - SunHeight;
   vec3 tint = mix(vec3(1.0, 1.0, 1.0),
                   vec3(1.2, 0.85, 0.60),
                   smoothstep(0.0, 0.4, warmth));
   terrainCol *= tint;

   /* Distance darkening
    *  Pixels far from camera get slightly darker.
    *  Complements the fog effect.                            */
   float distFactor = 1.0 - smoothstep(8.0, 18.0, Dist) * 0.3;
   terrainCol *= distFactor;

   //  Apply terrain colour and lighting
   gl_FragColor = vec4(terrainCol, 1.0) * color;
}