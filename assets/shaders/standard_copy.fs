#version 150

uniform sampler2D ImageSrc; // xyz=pos, w=enabled
uniform int effect;

// outputs
out vec4 colorOut;

void main()
{
    ivec2 iuv = ivec2(gl_FragCoord.xy); 
    switch (effect)
    {
        case 0:
            colorOut = texelFetch(ImageSrc, iuv, 0);
            break;
    
        case 1: // Depth buffer scaling
            colorOut = (texelFetch(ImageSrc, iuv, 0) - 1) * -500;
            break;
    }
}
