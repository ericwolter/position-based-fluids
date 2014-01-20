#version 150

uniform sampler2D depthTexture;
uniform mat4      invProjectionMatrix;

// outputs
out vec4 output; 

// Local variables
ivec2 frameSize; 
ivec2 iuv;

vec3 uvToEye(ivec2 texCoord)
{
    // sample depth buffer
    float z = texelFetch(depthTexture, texCoord, 0).x;
    
    // clipping
    if(z == gl_DepthRange.far) discard;
    
    // convert texture coordinate to homogeneous space
    vec2 xyPos = (texCoord + vec2(0.5)) / frameSize * 2.0 - 1.0;

    // construct clip-space position
    vec4 clipPos = vec4(xyPos, z, 1.0);

    // transform from clip space to view (eye) space
    vec4 viewPos = (invProjectionMatrix * clipPos);
    return (viewPos.xyz / viewPos.w);
}

void main()
{
    // Get frame coord
    iuv = ivec2(gl_FragCoord.xy);
    
    // get buffer size (we assume same buffer size for depthTexture and target)
    frameSize = textureSize(depthTexture, 0);
    
    // Calculate normal 
    vec3 eyePosition = uvToEye(iuv);

    vec3 ddx = uvToEye(iuv + ivec2(1, 0)) - eyePosition;
    vec3 ddx2 = eyePosition - uvToEye(iuv + ivec2(-1, 0));
    if( abs(ddx.z) > abs(ddx2.z) ) ddx = ddx2;

    vec3 ddy = uvToEye(iuv + ivec2(0, 1)) - eyePosition;
    vec3 ddy2 = eyePosition - uvToEye(iuv + ivec2(0, -1));
    if( abs(ddy.z) > abs(ddy2.z) ) ddy = ddy2;
    
    vec3 normal = normalize(cross(ddx, ddy));
    
    output = vec4(normal, 1);
}