__kernel void buildFriendsList(__constant struct Parameters* Params, 
                               const __global float4 *predicted,
                               const __global int *cells,
                               const __global int *particles_list,
                               __global int *friends_list,
                               const int N)
{
    const int i = get_global_id(0);
    if (i >= N) return;

    const int Circle0_offset  = i * PARTICLE_FRIENDS_BLOCK_SIZE + FRIENDS_CIRCLES;

    // Define circle particle counter varible
    int circleParticles[FRIENDS_CIRCLES];
    for (int j = 0; j < FRIENDS_CIRCLES; j++)
        circleParticles[j] = 0;
    
    // Start grid scan
    int3 current_cell = convert_int3(predicted[i].xyz * (float3)(Params->gridRes));
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            for (int z = -1; z <= 1; ++z)
            {
                uint cell_index = calcGridHash(current_cell + (int3)(x,y,z));

                // Next particle in list
                int next = cells[cell_index];
                while (next != END_OF_CELL_LIST)
                {
                    // next particle
                    int j_index = next;
                    next = particles_list[next];

                    // Skip self
                    if (i == j_index)
                        continue;
                    
                    // Ignore unfriendly particles (r > h)
                    float3 r = (predicted[i] - predicted[j_index]).xyz;
                    if (dot(r,r) >= Params->h_2)
                        continue;
                    
                    // Find particle circle
                    int j_circle = min(convert_int(length(r) * FRIENDS_CIRCLES / Params->h), FRIENDS_CIRCLES - 1);
                    
                    // Make sure particle doesn't have too many friends
                    if (circleParticles[j_circle] >= MAX_PARTICLES_IN_CIRCLE - 5)
                    {
                        // printf("Damn! we need a bigger MAX_PARTICLES_IN_CIRCLE\n");
                        continue;
                    }
                    //if (i == 0)
                    //    printf("j_index = %d, offset=%d\n", j_index, Circle0_offset + j_circle * MAX_PARTICLES_IN_CIRCLE + circleParticles[j_circle]);

                    // Add friend to relevent circle
                    friends_list[Circle0_offset + j_circle * MAX_PARTICLES_IN_CIRCLE + circleParticles[j_circle]] = j_index;
                    
                    circleParticles[j_circle]++;

                }
            }
        }
    }
    
    // Save counters
    for (int j = 0; j < FRIENDS_CIRCLES; j++)
        friends_list[i * PARTICLE_FRIENDS_BLOCK_SIZE + j] = circleParticles[j];
}