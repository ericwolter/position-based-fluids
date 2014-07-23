float3 BouncePointQuad(/*volatile __global int *debugBuf, */float3 PrevPos, float3 NextPos, float3 B, float3 E0, float3 E1, __read_only image2d_t surfacesMask, int maskYOffset, int maskHeight, float EdgeOffset)
{
    // Quad Precalc
    const float a = dot(E0, E0); 
    const float b = dot(E0, E1); 
    const float c = dot(E1, E1); 
    const float invdet = 1.0 / (a * c - b * b);    
    const float3 planeNorm  = normalize(cross(E0, E1)); 
    const float3 randOffset = planeNorm * EdgeOffset;

    // Compute factors
    const float3 D = B - NextPos;
    const float e = dot(E1, D);
    const float d = dot(E0, D);

    // Check if NextPos is in quad
    const float s = invdet * (b * e - c * d);
    const float t = invdet * (b * d - a * e);
    if ((s < 0) || (s > 1) || (t < 0) || (t > 1) /*|| (s + t > 1)*/)
        return NextPos;

    // Check NextPos if cull (should be "inside")
    const float3 planePos = B + E0 * s + E1 * t + randOffset;
    const float3 deltaP   = NextPos - planePos;
    if (dot(planeNorm, deltaP) > 0)
        return NextPos;

    // Check NextPos thickness (should be within force zone)
    float normal_velocity = dot(planeNorm, PrevPos - NextPos);
    if (length(deltaP) > EdgeOffset + normal_velocity)
        return NextPos;
        
    // Check against mask
    const int2 coord = (int2)(t * 512.0, maskYOffset + s * maskHeight);
    const int4 mask = read_imagei(surfacesMask, simpleSampler, coord);
    //logPrintf4(debugBuf, TextToID(Data=), mask.x, mask.y, mask.z, mask.w); 
    if (mask.x == 0)
        return NextPos;
        
    // Move point surface
    return planePos;
}

__kernel void computeDelta(__constant struct Parameters *Params,
                           volatile __global int *debugBuf,
                           __global float4 *delta,
                           const __global float4 *positions,
                           cbufferf_readonly imgPredicted, // xyz=predicted, w=scaling
                           const __global int *friends_list,
                           const float wave_generator,
                           __read_only image2d_t surfacesMask,
                           const int N)
{
    const int i = get_global_id(0);

    if (i >= N) return;
    
    // Read particle "i" position
    float4 particle_i = cbufferf_read(imgPredicted, i);
    
#ifdef LOCALMEM
    #define local_size       (256)

    const uint local_id = get_local_id(0);
    const uint group_id = get_group_id(0);

    // Load data into shared block
    __local float4 loc_predicted[local_size]; 
    loc_predicted[local_id] = particle_i;
    barrier(CLK_LOCAL_MEM_FENCE);
#endif

    uint2 randSeed = (uint2)(1 + get_global_id(0), 1);

    // Sum of lambdas
    float3 sum = (float3)0.0f;
    const float h_2_cache = Params->h_2;
    const float h_cache = Params->h;

    // equation (13)
    const float q_2 = pow(Params->surfaceTenstionDist * h_cache, 2);
    const float poly6_q = pow(h_2_cache - q_2, 3);

#ifdef LOCALMEM
    int localHit =0;
    int localMiss = 0;
#endif

    // read number of friends
    int totalFriends = 0;
    int circleParticles[MAX_FRIENDS_CIRCLES];
    for (int j = 0; j < MAX_FRIENDS_CIRCLES; j++)
        totalFriends += circleParticles[j] = friends_list[j * MAX_PARTICLES_COUNT + i];

    int proccedFriends = 0;

    for (int iCircle = 0; iCircle < MAX_FRIENDS_CIRCLES; iCircle++)
    {
        // Check if we want to process/skip next friends circle
        if (((float)proccedFriends) / totalFriends > 0.5f)
            continue;

        // Add next circle to process count
        proccedFriends += circleParticles[iCircle];

        // Compute friends start offset
        int baseIndex = FRIENDS_BLOCK_SIZE +                                      // Skip friendsCount block
                        iCircle * (MAX_PARTICLES_COUNT * MAX_FRIENDS_IN_CIRCLE) + // Offset to relevent circle
                        i;   

        // Process friends in circle
        for (int iFriend = 0; iFriend < circleParticles[iCircle]; iFriend++)
        {
            // Read friend index from friends_list
            const int j_index = friends_list[baseIndex + iFriend * MAX_PARTICLES_COUNT];

            // Get particle "j" position
#ifdef LOCALMEM
            float4 particle_j;
            if (j_index / local_size == group_id)
            {
                particle_j = loc_predicted[j_index % local_size];
            //     localHit++;
            //     atomic_inc(&stats[0]);
            }
            else
            {
                particle_j = cbufferf_read(imgPredicted, j_index);
            //     localMiss++;
            //     atomic_inc(&stats[1]);
            }
#else
            const float4 particle_j = cbufferf_read(imgPredicted, j_index);
#endif

            // Compute r, length(r) and length(r)^2
            const float3 r         = particle_i.xyz - particle_j.xyz;
            const float r_length_2 = dot(r, r);

            if (r_length_2 < h_2_cache)
            {
                const float r_length   = sqrt(r_length_2);

                const float3 gradient_spiky = r / (r_length) *
                                              (h_cache - r_length) *
                                              (h_cache - r_length);

                const float r_2_diff = h_2_cache - r_length_2;
                const float poly6_r = r_2_diff * r_2_diff * r_2_diff;

                const float r_q_radio = poly6_r / poly6_q;
                const float s_corr = Params->surfaceTenstionK * r_q_radio * r_q_radio * r_q_radio * r_q_radio;

                // Sum for delta p of scaling factors and grad spiky (equation 12)
                sum += (particle_i.w + particle_j.w + s_corr) * gradient_spiky;
            }
        }
    }

    // equation (12)
    float3 delta_p = (-GRAD_SPIKY_FACTOR*sum) / Params->restDensity;
    float3 future = particle_i.xyz + delta_p;

    // Compute edge offset
    float3 noisePos = future * 5 / Params->h;
    float edgeOffset = 1.0+(3 + sin(noisePos.x)+sin(noisePos.y)+sin(noisePos.z)) * Params->h * 0.03f;

    // Force plats (copy paste this section)
    future = BouncePointQuad(positions[i].xyz, future, (float3)(  -165.8,   -106.4,    28.32), (float3)(  -15.39,    41.82,        0), (float3)(   52.28,    19.23,        0), surfacesMask, 0, 409, edgeOffset);
    future = BouncePointQuad(positions[i].xyz, future, (float3)(  -158.5,   -126.3,   -37.11), (float3)(  -7.638,    20.76,     66.1), (float3)(   52.05,    19.15,        0), surfacesMask, 409, 643, edgeOffset);
    future = BouncePointQuad(positions[i].xyz, future, (float3)(  -21.79,   -23.62,   -34.55), (float3)(   89.42,    39.44,        0), (float3)(  -27.26,    61.81,        0), surfacesMask, 1052, 740, edgeOffset);
    future = BouncePointQuad(positions[i].xyz, future, (float3)(  -39.96,   -11.55,    29.95), (float3)(  -8.877,    49.84,        0), (float3)(   103.3,     18.4,        0), surfacesMask, 1792, 247, edgeOffset);
    future = BouncePointQuad(positions[i].xyz, future, (float3)(  -180.5,    -64.4,   -37.25), (float3)(-0.0009671, -0.0003558,    66.38), (float3)(   22.87,   -62.17,        0), surfacesMask, 2039, 512, edgeOffset);
    future = BouncePointQuad(positions[i].xyz, future, (float3)(    35.5,    2.278,   -35.25), (float3)(       0,        0,    65.91), (float3)(   21.74,    36.07,        0), surfacesMask, 2551, 801, edgeOffset);
    future = BouncePointQuad(positions[i].xyz, future, (float3)(  -31.54,    1.768,   -8.499), (float3)(       0,        0,    15.86), (float3)(  -15.69,   -32.17,        0), surfacesMask, 3352, 226, edgeOffset);
    future = BouncePointQuad(positions[i].xyz, future, (float3)(  -67.22,   -41.65,    7.364), (float3)(   11.45,        0,        0), (float3)(       0,        0,   -15.86), surfacesMask, 3578, 369, edgeOffset);
    future = BouncePointQuad(positions[i].xyz, future, (float3)(  -65.91,   -42.02,    7.364), (float3)(       0,        0,   -15.87), (float3)(  -7.275,    4.603,        0), surfacesMask, 3947, 943, edgeOffset);
    future = BouncePointQuad(positions[i].xyz, future, (float3)(   -9.05,   -4.783,   -8.792), (float3)(  -47.98,    -37.3,        0), (float3)(       0,        0,    16.45), surfacesMask, 4890, 1891, edgeOffset);
    future = BouncePointQuad(positions[i].xyz, future, (float3)(  -53.98,   -25.37,    6.656), (float3)(   27.86,    34.59,        0), (float3)(   17.42,   -14.03,        0), surfacesMask, 6781, 1016, edgeOffset);
    future = BouncePointQuad(positions[i].xyz, future, (float3)(  -48.37,   -30.96,   -7.792), (float3)(   21.75,   -10.61,        0), (float3)(   17.92,    36.76,        0), surfacesMask, 7797, 303, edgeOffset);
    future = BouncePointQuad(positions[i].xyz, future, (float3)(  -48.28,    38.39,   -35.25), (float3)(       0,        0,    65.91), (float3)(   20.93,   -47.46,        0), surfacesMask, 8100, 650, edgeOffset);
    future = BouncePointQuad(positions[i].xyz, future, (float3)(   36.56,    3.007,    30.66), (float3)(       0,        0,   -65.91), (float3)(  -64.89,   -11.56,        0), surfacesMask, 8750, 512, edgeOffset);

    // Compute delta
    delta[i].xyz = future - particle_i.xyz;
}
