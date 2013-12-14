#version 130

uniform sampler2D SourceImg; // xyz=pos, w=enabled

void main()
{
    ivec2 iuv = ivec2(gl_FragCoord.xy); 
    gl_FragData[0] = texelFetch(SourceImg, iuv, 0);
    gl_FragData[0].w = 1;//vec4(1,0,0,1);
}
