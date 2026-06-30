// PER PIXEL LIGHTING 

//  Original per-pixel cloth shading shader.  Demonstrates granular
//  per-pixel shading changes . 
//
//  1. WAVE-CREST BRIGHTENING -- pixels at wave crests are brighter,
//     troughs are darker.  This only works per-pixel because each pixel
//     receives its own WaveZ value interpolated from the vertex shader.
//
//  2. TWO-SIDED CLOTH LIGHTING -- front face is lit normally; back face
//     (when the flag folds away from the camera) gets a cooler, darker
//     tone like the underside of real fabric.
//
//  3. FLAG DESIGN ZONES -- the flag is divided into three vertical
//     stripes (left=white, centre=red with darker midband, right=white)
//     using TexCoord.x.  Each zone gets independent colour and shading.
//
//  4. SILKY SPECULAR -- wide low-intensity specular (shininess=8) so
//     the highlight spreads broadly across the cloth like silk/nylon
//     rather than the sharp point of plastic or metal.
//
//  5. EDGE DARKENING -- pixels near the flag edges (TexCoord near 0 or 1)
//     are slightly darker, simulating the shadow that falls in the
//     fold/hem of real cloth.
#version 120

varying vec3  Light;
varying vec3  Light2;
varying vec3  Normal;
varying vec3  View;
varying vec2  TexCoord;
varying float WaveZ;

void main()
{
   //  Normalise all incoming vectors
   vec3 N = normalize(Normal);
   vec3 L = normalize(Light);
   vec3 V = normalize(View);

   //  Two-sided lighting: flip normal for back faces so cloth underside
   //  shades differently from the front .
   if (!gl_FrontFacing) N = -N;

   //  Reflected light vector for specular
   vec3 R = reflect(-L, N);

   //  Sun diffuse -- standard Lambert  
   float Id = max(dot(L, N), 0.0);

   //  Silky specular -- shininess 8 gives a broad soft highlight
   //  appropriate for fabric rather than the sharp point of plastic 
   float Is = (Id > 0.0) ? pow(max(dot(R, V), 0.0), 8.0) : 0.0;

   //  Headlight contribution (second light)
   vec3  L2  = normalize(Light2);
   vec3  R2  = reflect(-L2, N);
   float Id2 = max(dot(L2, N), 0.0);
   float Is2 = (Id2 > 0.0) ? pow(max(dot(R2, V), 0.0), 8.0) : 0.0;

   //  Base lighting sum from sun 
   vec4 lighting = gl_FrontMaterial.emission
                 + gl_FrontLightProduct[0].ambient
                 + Id  * gl_FrontLightProduct[0].diffuse
                 + Is  * vec4(0.35, 0.35, 0.35, 1.0)
                 + Id2 * gl_FrontLightProduct[1].diffuse
                 + Is2 * vec4(0.20, 0.20, 0.20, 1.0);

   //  FLAG DESIGN ZONES using TexCoord.x 
   //  Left stripe (0.0 - 0.18): white
   //  Centre band (0.18 - 0.82): red with darker midband
   //  Right stripe (0.82 - 1.0): white
   vec3 flagCol;
   float u = TexCoord.x;
   float v = TexCoord.y;

   if (u < 0.18 || u > 0.82)
   {
      //  White stripes on left and right edges
      flagCol = vec3(0.95, 0.95, 0.95);
   }
   else
   {
      //  Red centre ,slightly darker horizontal band through the middle
      //  gives the impression of a design/stripe on the cloth 
      float midDark = 1.0 - 0.25 * smoothstep(0.35, 0.50, v)
                          * (1.0 - smoothstep(0.50, 0.65, v));
      flagCol = vec3(0.85 * midDark, 0.08 * midDark, 0.08 * midDark);
   }

   //  WAVE-CREST BRIGHTENING 
   //  WaveZ is positive at crests, negative at troughs.
   //  Normalise to [-1, 1] range (max wave displacement is ~0.10)
   //  and use it to slightly brighten crests and darken troughs.
   //  This is only possible per-pixel because each pixel gets its own
   //  interpolated WaveZ value -- vertex lighting would average this
   //  away completely.
   float waveTint = 1.0 + 0.25 * clamp(WaveZ / 0.10, -1.0, 1.0);
   flagCol *= waveTint;

   //  TWO-SIDED COLOUR DIFFERENCE 
   //  Back face gets a cooler, slightly blue-shifted darker tone
   //  so the underside of the cloth looks different from the front,
   //  exactly like real woven fabric viewed from behind.
   if (!gl_FrontFacing)
      flagCol = mix(flagCol, vec3(0.40, 0.40, 0.55), 0.35);

   //  EDGE DARKENING
   //  Pixels near the top/bottom edges (hem of cloth) are slightly
   //  darker, simulating the shadow that falls in real fabric folds.
   float edgeDark = 1.0 - 0.30 * (1.0 - smoothstep(0.0, 0.12, v))
                        - 0.20 * smoothstep(0.88, 1.0, v);
   flagCol *= edgeDark;

   //  Final colour: flag design * per-pixel lighting
   gl_FragColor = vec4(flagCol, 1.0) * lighting;
}
