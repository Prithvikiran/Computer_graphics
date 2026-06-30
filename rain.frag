
//  Colours each raindrop as a semi-transparent blue-white streak.
#version 120

varying float Alpha;

void main()
{
   //  Blue-white raindrop colour, semi-transparent
   gl_FragColor = vec4(0.75, 0.85, 1.0, Alpha * 0.6);
}
