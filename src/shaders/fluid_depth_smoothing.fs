#version 150

uniform sampler2D depthTexture;
uniform sampler2D particlesPos;
uniform mat4      MV_Matrix;
uniform mat4      iMV_Matrix;
uniform mat4      Proj_Matrix;
uniform vec2      depthRange;
uniform vec2      invFocalLen; // See http://stackoverflow.com/questions/17647222/ssao-changing-dramatically-with-camera-angle

// Particles related
uniform int       particlesCount;

// outputs
out vec4 output; 

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

vec3 uvToViewSpace(ivec2 texCoord)
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
    return homoPos.xyz / homoPos.w;
}

float ViewSpaceDepth_to_ZBufferDepth(vec3 viewSpacePos)
{
    // convert to ClipSpace
    vec4 clipSpacePos = Proj_Matrix * vec4(viewSpacePos, 1.0);
    float ndcDepth    = clipSpacePos.z/clipSpacePos.w;
    
    // Clip adjusted-z
    if (ndcDepth < -1.0)
        discard;
 
    // Transform into window coordinates coordinates 
    float near = 0;
    float far  = 1;
    return (abs(far - near) * ndcDepth + near + far) / 2.0;
}

vec3 GetParticlePos(int index)
{
    // Compute texture location
    ivec2 texSize = textureSize(particlesPos, 0);
    ivec2 texCoord = ivec2(index % texSize.x, index / texSize.x);
    
    // Get position value
    return texelFetch(particlesPos, texCoord, 0).xyz;
}

float GetDensity(vec3 worldPos)
{
    float h = 0.035;
    float h_2 = h*h;
    
    float density = 0.0;
    for (int i = 0; i < particlesCount; i++)
    {
        // Get particles position
        vec3 partPos = GetParticlePos(i);
        
        // find distance^2 between pixel and particles
        vec3 delta = partPos - worldPos;
        float r_2 = dot(delta, delta);
        
        // Check if out of range
        if (r_2 > h_2)
            continue;
            
        // append density
        float h_2_r_2_diff = h_2 - r_2;
        density += h_2_r_2_diff * h_2_r_2_diff * h_2_r_2_diff;    
    }
    
    return density;
}

void main()
{
    // Get frame coord
    iuv = ivec2(gl_FragCoord.xy);
    
    // get buffer size (we assume same buffer size for depthTexture and target)
    frameSize = textureSize(depthTexture, 0);
    
    // get camera position (model space)
    vec3 cameraPos = iMV_Matrix[3].xyz;
    
    // get pixels position (model space)
    vec3 modelPos = uvToWorld(iuv).xyz;
    
    // compute pixel-camera normal (model space)
    vec3 pixelCamNorm = normalize(cameraPos - modelPos);
    
    // Scan for closest density along camera normal
    float scanDist = 0.035 / 2.0;
    float scanStep = scanDist / 6;
    float TargetDensity = 1.0 / 400000000.0;
    
    vec3  currPos = modelPos;
    float currDensity = 1.0E10;
    vec3  prevPos = currPos;
    float prevDensity = currDensity;
    for (float tp = -scanDist; tp < scanDist; tp += scanStep)
    {
        // Compute shifted test point
        prevPos = currPos;
        currPos = modelPos + tp * pixelCamNorm;
        
        // get test point density
        prevDensity = currDensity;
        currDensity = GetDensity(currPos);
        
        // Exit when we cross the threshold
        if (currDensity < TargetDensity)
            break;
    }
    
    // Interpolate position
    float ratio = (TargetDensity - currDensity) / (prevDensity - currDensity);
    vec3 shellPos = mix(currPos, prevPos, ratio);
   
    // Convert back to viewspace
    vec3 viewSpaceDepth = (MV_Matrix * vec4(shellPos, 1.0)).xyz;
    
    // Convert viewspace to z-buffer depth
    float zbufferDepth = ViewSpaceDepth_to_ZBufferDepth(viewSpaceDepth);
    
    // Compose result
    output = vec4(zbufferDepth, 0, 0, 1);
    
    // Test: input depth, output depth, vsInput, vsOutput 
    // float zbufferInput = texelFetch(depthTexture, iuv, 0).x;
    // vec3 vsPos = uvToViewSpace(iuv);
    // output = vec4(zbufferInput, zbufferDepth, vsPos.z, viewSpaceDepth.z);
}