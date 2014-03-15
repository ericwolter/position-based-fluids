#version 150

uniform sampler2D depthTexture;
uniform mat4      invProjectionMatrix;

uniform mat4      iMV_Matrix;
uniform vec2      depthRange;
uniform vec2      invFocalLen; 

// outputs
out vec4 result; 

// Local variables
ivec2 frameSize; 
ivec2 iuv;

vec4 uvToWorld(ivec2 texCoord)
{
    // sample depth buffer
    float z = texelFetch(depthTexture, texCoord, 0).x;
    
    // clipping
    if(z == gl_DepthRange.far) discard;
    
    // Linearise "z"
    float near = depthRange.x;
    float far = depthRange.y;
    float linearZ = near / (far - z * (far - near)) * far;
    
    // convert texture coordinate to -invFocalLen .. +invFocalLen
    vec2 focal_uv = ((texCoord + vec2(0.5)) / frameSize * 2.0 - 1.0) * invFocalLen;

    // homogeneous space coordinates
    vec4 homoPos = vec4(focal_uv * linearZ, -linearZ, 1.0);
    
    // view space coords 
    // vsPos = homoPos.xyz / homoPos.w;

    // inverse to model coords
    vec4 ret = iMV_Matrix * homoPos;
    return ret ;
}

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

vec3 DoLight(vec3 normal, vec3 eyeDir, vec3 lightDir)
{
    vec3 halfWay = normalize(lightDir+eyeDir); //Halfway vector
    
    vec3 diffuseReflection  = vec3(0,0,0.5)*clamp(dot(lightDir, normal), 0, 1);
    vec3 specularReflection = vec3(1)*max(0.0,pow(dot(normal, halfWay), 50));
    
    //Fresnel approximation
    float fZero = pow( (1.0f-(1.0f/1.31f))/ (1.0f+(1.0f/1.31f)), 2);
    float base = 1-dot(eyeDir, halfWay);
    float exp = pow(base, 5);
    float fresnel = fZero + (1 - fZero)*exp;
     
    return diffuseReflection+specularReflection/*fresnel*50*/;
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
    
    normal = texelFetch(depthTexture, iuv, 0).yzw;
    //normal = uvToWorld(iuv).xyz;

    // get camera position (model space)
    vec3 cameraPos = iMV_Matrix[3].xyz;    
    vec3 lightDir = normalize(vec3(0,1,0));
    
    
    result = vec4(DoLight(normal, normalize(cameraPos - uvToWorld(iuv).xyz), lightDir), 1.0);
}