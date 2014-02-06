__kernel void computeDelta(__constant struct Parameters *Params,
                           volatile __global int *debugBuf,
                           __global float4 *delta,
                           const __global float4 *predicted, // xyz=predicted, w=scaling
                           const __global int *friends_list,
                           const float wave_generator,
                           const int N)
{
    const int i = get_global_id(0);
    if (i >= N) return;

    const size_t local_size = 32;

    // Load data into shared block
    float4 i_data = predicted[i];
    __local float4 loc_predicted[32]; //size=local_size*4*4
    loc_predicted[get_local_id(0)] = i_data;
    barrier(CLK_LOCAL_MEM_FENCE);    

    uint2 randSeed = (uint2)(1 + get_global_id(0), 1);

    // Sum of lambdas
    float3 sum = (float3)0.0f;
    const float h_2_cache = Params->h_2;
    const float h_cache = Params->h;

    // equation (13)
    const float q_2 = pow(Params->surfaceTenstionDist * h_cache, 2);
    const float poly6_q = pow(h_2_cache - q_2, 3);

    // read number of friends
    int totalFriends = 0;
    int circleParticles[FRIENDS_CIRCLES];
    for (int j = 0; j < FRIENDS_CIRCLES; j++)
        totalFriends += circleParticles[j] = friends_list[i * PARTICLE_FRIENDS_BLOCK_SIZE + j];

    int proccedFriends = 0;
    for (int iCircle = 0; iCircle < FRIENDS_CIRCLES; iCircle++)
    {
        // Check if we want to process/skip next friends circle
        if (((float)proccedFriends) / totalFriends > 0.5f)
            continue;

        // Add next circle to process count
        proccedFriends += circleParticles[iCircle];

        // Compute friends list start offset
        int baseIndex = i * PARTICLE_FRIENDS_BLOCK_SIZE + FRIENDS_CIRCLES +   // Offset to first circle -> "circle[0]"
                        iCircle * MAX_PARTICLES_IN_CIRCLE;                    // Offset to iCircle      -> "circle[iCircle]"

        // Process friends in circle
        for (int iFriend = 0; iFriend < circleParticles[iCircle]; iFriend++)
        {
            // Read friend index from friends_list
            const int j_index = friends_list[baseIndex + iFriend];

            // Get j particle data
            // const float4 j_data = predicted[j_index];
            float4 j_data;
            if (j_index / local_size == get_group_id(0))
                j_data = loc_predicted[j_index % local_size];
            else
                j_data = predicted[j_index];

            // Compute r, length(r) and length(r)^2
            const float3 r         = predicted[i].xyz - j_data.xyz;
            const float r_length_2 = dot(r, r);
            
            if (r_length_2 < h_2_cache)
            {
                const float r_length   = sqrt(r_length_2);

                const float3 gradient_spiky = -1.0f * GRAD_SPIKY_FACTOR *
                                              r / (r_length) *
                                              (h_cache - r_length) *
                                              (h_cache - r_length);

                const float r_2_diff = h_2_cache - r_length_2;
                const float poly6_r = r_2_diff * r_2_diff * r_2_diff;

                const float r_q_radio = poly6_r / poly6_q;
                const float s_corr = Params->surfaceTenstionK * r_q_radio * r_q_radio * r_q_radio * r_q_radio;

                // Sum for delta p of scaling factors and grad spiky (equation 12)
                sum += (predicted[i].w + j_data.w + s_corr) * gradient_spiky;
            }
        }
    }

    // equation (12)
    float3 delta_p = sum / Params->restDensity;

    float randDist = 0.005f;
    float3 future = predicted[i].xyz + delta_p;

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
    delta[i].xyz = future - predicted[i].xyz;

    // #if defined(USE_DEBUG)
    //printf("compute_delta: result: i: %d (N=%d)\ndelta: [%f,%f,%f,%f]\n",
    //      i, NeighborCount,
    //      delta[i].x, delta[i].y, delta[i].z, minR);
    //printf("Particle i=%d: Neighbors=%d/%d ClosestParticle=%fh\n", i, NeighborCount, ScanCount, minR/Params->h);
    // #endif
}
