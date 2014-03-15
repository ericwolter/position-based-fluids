#version 150

uniform sampler2D SourceImg; // xyz=pos, w=enabled

// outputs
out vec4 colorOut;

void main()
{
    ivec2 iuv = ivec2(gl_FragCoord.xy); 
    colorOut = texelFetch(SourceImg, iuv, 0);
}
