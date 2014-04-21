#version 430

uniform sampler2D depthTexture;

layout (binding = 0, rgba32f) uniform readonly imageBuffer  imgPosition;
layout (binding = 1, r32i)    uniform readonly iimageBuffer imgCells;

// Projection and Deprojection
uniform mat4      MV_Matrix;
uniform mat4      iMV_Matrix;
uniform mat4      Proj_Matrix;
uniform vec2      depthRange;
uniform vec2      invFocalLen; // See http://stackoverflow.com/questions/17647222/ssao-changing-dramatically-with-camera-angle

// Particles related
uniform float     smoothLength;
uniform int       particlesCount;
uniform uint      currentCycleID;

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

int expandBits(int x)
{
    x = (x | (x << 16)) & 0x030000FF;
    x = (x | (x <<  8)) & 0x0300F00F;
    x = (x | (x <<  4)) & 0x030C30C3;
    x = (x | (x <<  2)) & 0x09249249;

    return x;
}

int mortonNumber(ivec3 gridPos)
{
    return expandBits(gridPos.x) | (expandBits(gridPos.y) << 1) | (expandBits(gridPos.z) << 2);
}

int calcGridHash(ivec3 gridPos)
{
    return mortonNumber(gridPos) % 200000/*GRID_BUF_SIZE*/;
}

void SampleDensityAndGradient(vec3 worldPos, out float density, out vec3 gradient)
{
    // Cash h^2
    float h_2 = smoothLength*smoothLength;

    // Reset density sum
    density = 0.0;
    gradient = vec3(0);
    int count = 0;
    
    // Compute Grid position
    ivec3 centerCell = ivec3(worldPos / smoothLength);
result =vec4(1);
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            for (int z = -1; z <= 1; ++z)
            {
                // find first and last particle in this cell
                int cell_index = calcGridHash(centerCell + ivec3(x, y, z));
                int startIndex = imageLoad(imgCells, cell_index * 2 + 0).x;
                int endIndex   = imageLoad(imgCells, cell_index * 2 + 1).x;
                result = vec4(startIndex,0,0,1); return;

                // Scan all particles in cell
                for (int iPart = startIndex; iPart <= endIndex; iPart++)  
                {
                    // Get particles position
                    vec3 partPos = imageLoad(imgPosition, iPart).xyz;
                    
                    // find distance^2 between pixel and particles
                    vec3 delta = partPos - worldPos;
                    float r_2 = dot(delta, delta);
                    
                    // Check if out of range
                    if (r_2 < h_2)
                    {
                        // count in-range particles
                        count++;

                        // sum gradient
                        float h_2_r_2_diff = h_2 - r_2;
                        gradient += delta * h_2_r_2_diff * h_2_r_2_diff;

                        // append density
                        density += h_2_r_2_diff * h_2_r_2_diff * h_2_r_2_diff;    
                    }
                }
            }
        }
    }

    // scale by factors
    density  *=  1; // poly6Factor;
    gradient *= -6; // poly6GradFactor;
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
    
    // Select shell target density
    float TargetDensity = 1.0 / 400000000.0;
    
    // define variables
    float density;
    vec3  gradient = vec3(0);

    // Start scanning for shell sweet spot
    float tp = 0.0;
    vec3 shellPos = modelPos;
    for (int iIter = 0; iIter < 2; iIter++)
    {
        // get test point density
        SampleDensityAndGradient(shellPos, density, gradient);
        return;
        
        // Compute density delta 
        float densityDelta = density - TargetDensity;

        // Compute gradient along camera normal
        float gradientAlongCamNormal = dot(pixelCamNorm, gradient);
        
        // Compute step size
        float stepSize = densityDelta / gradientAlongCamNormal;
        
        // update tp
        tp += stepSize; 

        // Compute shifted test point
        shellPos = modelPos + tp * pixelCamNorm;
        
    }
    
    // Convert back to viewspace
    vec3 viewSpaceDepth = (MV_Matrix * vec4(shellPos, 1.0)).xyz;
    
    // Convert viewspace to z-buffer depth
    float zbufferDepth = ViewSpaceDepth_to_ZBufferDepth(viewSpaceDepth);
    
    // Compose result
    result = vec4(zbufferDepth, normalize(gradient));
}