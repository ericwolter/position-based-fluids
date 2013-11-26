uint rand(uint2 *state)
{
    enum { A=4294883355U};
    uint x=(*state).x, c=(*state).y;  // Unpack the state
    uint res=x^c;                     // Calculate the result
    uint hi=mul_hi(x,A);              // Step the RNG
    x=x*A+c;
    c=hi+(x<c);
    *state=(uint2)(x,c);              // Pack the state back up
    return res;                       // Return the next result
}

float frand(uint2 *state)
{
    return rand(state) / 4294967295.0f;
}

float frand3(float3 co)
{
    float ipr;
    return fract(sin(dot(co.xy, (float2)(12.9898,78.233))) * (43758.5453 + co.z), &ipr);
}

float rand_3d(float3 pos)
{
    float3 pos_i;
    float3 pos_f = fract(pos, &pos_i);
    
    // Calculate noise contributions from each of the eight corners
    float n000 = frand3(pos_i + (float3)(0,0,0));
    float n100 = frand3(pos_i + (float3)(1,0,0));
    float n010 = frand3(pos_i + (float3)(0,1,0));
    float n110 = frand3(pos_i + (float3)(1,1,0));
    float n001 = frand3(pos_i + (float3)(0,0,1));
    float n101 = frand3(pos_i + (float3)(1,0,1));
    float n011 = frand3(pos_i + (float3)(0,1,1));
    float n111 = frand3(pos_i + (float3)(1,1,1));
    
    // Compute the fade curve value for each of x, y, z
    float u = smoothstep(0.0f, 1.0f, pos_f.x);
    float v = smoothstep(0.0f, 1.0f, pos_f.y);
    float w = smoothstep(0.0f, 1.0f, pos_f.z);
    
    // Interpolate along x the contributions from each of the corners
    float nx00 = mix(n000, n100, u);
    float nx01 = mix(n001, n101, u);
    float nx10 = mix(n010, n110, u);
    float nx11 = mix(n011, n111, u);
     
    // Interpolate the four results along y
    float nxy0 = mix(nx00, nx10, v);
    float nxy1 = mix(nx01, nx11, v);
     
    // Interpolate the two last results along z
    float nxyz = mix(nxy0, nxy1, w);
    
    return nxyz;
}


__kernel void computeDelta(__constant struct Parameters* Params, 
                           __global float4 *delta,
                           const __global float4 *predicted,
                           const __global float *scaling,
                           const __global int *friends_list,
                           const float wave_generator,
                           const int N)
{
    const int i = get_global_id(0);
    if (i >= N) return;

    uint2 randSeed = (uint2)(1+get_global_id(0), 1);

    int3 current_cell = convert_int3(predicted[i].xyz * (float3)(Params->gridRes));

    // Sum of lambdas
    float4 sum = (float4)0.0f;

    // read number of friends
    int circleParticles[FRIENDS_CIRCLES];
    for (int j = 0; j < FRIENDS_CIRCLES; j++)
        circleParticles[j] = friends_list[i * PARTICLE_FRIENDS_BLOCK_SIZE + j];    

    for (int iCircle = 0; iCircle < FRIENDS_CIRCLES; iCircle++)
    {
        for (int iFriend = 0; iFriend < circleParticles[iCircle]; iFriend++)
        {
            // Read friend index from friends_list
            int j_index = friends_list[i * PARTICLE_FRIENDS_BLOCK_SIZE + FRIENDS_CIRCLES +   // Offset to first circle -> "circle[0]"
                                       iCircle * MAX_PARTICLES_IN_CIRCLE +                   // Offset to iCircle      -> "circle[iCircle]"
                                       iFriend];                                             // Offset to iFriend      -> "circle[iCircle][iFriend]"
        
            float4 r = predicted[i] - predicted[j_index];
            float r_length_2 = r.x * r.x + r.y * r.y + r.z * r.z;

            if (r_length_2 > 0.0f && r_length_2 < Params->h_2)
            {
                float r_length = sqrt(r_length_2);
                float4 gradient_spiky = -1.0f * r / (r_length)
                                        * GRAD_SPIKY_FACTOR
                                        * (Params->h - r_length)
                                        * (Params->h - r_length);

                float poly6_r = POLY6_FACTOR * (Params->h_2 - r_length_2) * (Params->h_2 - r_length_2) * (Params->h_2 - r_length_2);

                // equation (13)
                const float q_2 = pow(Params->surfaceTenstionDist * Params->h, 2);
                float poly6_q = POLY6_FACTOR * (Params->h_2 - q_2) * (Params->h_2 - q_2) * (Params->h_2 - q_2);
                const uint n = 4;

                float s_corr = -1.0f * Params->surfaceTenstionK * pow(poly6_r / poly6_q, n);

                // Sum for delta p of scaling factors and grad spiky
                // in equation (12)

                sum += (scaling[i] + scaling[j_index] + s_corr) * gradient_spiky;
            }
        }
    }

    // equation (12)
    float4 delta_p = sum / Params->restDensity;

    float randDist = 0.005f;
    float4 future = predicted[i] + delta_p;

    // Prime the random... DO NOT REMOVE
    frand(&randSeed);
    frand(&randSeed);
    frand(&randSeed);

    // Clamp Y
    if      (future.y < Params->yMin) future.y = Params->yMin + frand(&randSeed) * randDist;
    if      (future.z < Params->zMin) future.z = Params->zMin + frand(&randSeed) * randDist;
    else if (future.z > Params->zMax) future.z = Params->zMax - frand(&randSeed) * randDist;
    if      (future.x < (Params->xMin + wave_generator))  future.x = Params->xMin + wave_generator + frand(&randSeed) * randDist;
    else if (future.x > (Params->xMax                 ))  future.x = Params->xMax                  - frand(&randSeed) * randDist;

	// Compute delta
    delta[i] = future - predicted[i];

    // #if defined(USE_DEBUG)
    //printf("compute_delta: result: i: %d (N=%d)\ndelta: [%f,%f,%f,%f]\n",
    //      i, NeighborCount,
    //      delta[i].x, delta[i].y, delta[i].z, minR);
    //printf("Particle i=%d: Neighbors=%d/%d ClosestParticle=%fh\n", i, NeighborCount, ScanCount, minR/Params->h);
    // #endif
}
