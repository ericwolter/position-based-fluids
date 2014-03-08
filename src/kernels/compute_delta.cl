__kernel void computeDelta(__constant struct Parameters *Params,
                           volatile __global int *debugBuf,
                           __global float4 *delta,
                           const __global float4 *predicted, // xyz=predicted, w=scaling
                           const __global int *friends_list,
                           __global image2d_t img_friends_list,
                           const float wave_generator,
                           const int N)
{
    const int i = get_global_id(0);

    if (i >= N) return;
    
#ifdef LOCALMEM
    #define local_size       (256)

    const uint local_id = get_local_id(0);
    const uint group_id = get_group_id(0);

    // Load data into shared block
    __local float4 loc_predicted[local_size]; 
    loc_predicted[local_id] = predicted[i];
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

    int localHit =0;
    int localMiss = 0;

    // read number of friends
    int totalFriends = 0;
    int circleParticles[MAX_FRIENDS_CIRCLES];
    for (int j = 0; j < MAX_FRIENDS_CIRCLES; j++)
        totalFriends += circleParticles[j] = friends_list[j * MAX_PARTICLES_COUNT + i];
//        totalFriends += circleParticles[j] = imgReadui1(img_friends_list, j * MAX_PARTICLES_COUNT + i);

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
            //const int j_index = imgReadui1(img_friends_list, baseIndex + iFriend * MAX_PARTICLES_COUNT);
            const int j_index = friends_list[baseIndex + iFriend * MAX_PARTICLES_COUNT];

            // Get j particle data
#ifdef LOCALMEM
            float4 j_data;
            if (j_index / local_size == group_id)
            {
                j_data = loc_predicted[j_index % local_size];
            //     localHit++;
            //     atomic_inc(&stats[0]);
            }
            else
            {
                j_data = predicted[j_index];
            //     localMiss++;
            //     atomic_inc(&stats[1]);
            }
#else
            const float4 j_data = predicted[j_index];
#endif

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
    if (future.y < Params->yMin) future.y = Params->yMin + frand(&randSeed) * randDist;
    if (future.z < Params->zMin) future.z = Params->zMin + frand(&randSeed) * randDist;
    else if (future.z > Params->zMax) future.z = Params->zMax - frand(&randSeed) * randDist;
    if (future.x < (Params->xMin + wave_generator))  future.x = Params->xMin + wave_generator + frand(&randSeed) * randDist;
    else if (future.x > (Params->xMax))  future.x = Params->xMax                  - frand(&randSeed) * randDist;

    // Compute delta
    delta[i].xyz = future - predicted[i].xyz;

//    if(group_id == 0) {
  //      printf("%d: hits: %d vs miss: %d\n", i, localHit,localMiss);
    //}

    // #if defined(USE_DEBUG)
    //printf("compute_delta: result: i: %d (N=%d)\ndelta: [%f,%f,%f,%f]\n",
    //      i, NeighborCount,
    //      delta[i].x, delta[i].y, delta[i].z, minR);
    //printf("Particle i=%d: Neighbors=%d/%d ClosestParticle=%fh\n", i, NeighborCount, ScanCount, minR/Params->h);
    // #endif
}
