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

__kernel void computeDelta(__global float4 *delta,
                           const __global float4 *predicted,
                           const __global float *scaling,
                           const __global int *cells,
                           const __global int *particles_list,
                           const float wave_generator,
                           const int N)
{
    const int i = get_global_id(0);
    if (i >= N) return;

    const int END_OF_CELL_LIST = -1;

    uint2 randSeed = (uint2)(1+get_global_id(0), 1);

    int3 current_cell = 100 + convert_int3(predicted[i].xyz * (float3)(NUMBER_OF_CELLS_X, NUMBER_OF_CELLS_Y, NUMBER_OF_CELLS_Z));

    // Sum of lambdas
    float4 sum = (float4) 0.0f;
    float minR = 100.0f;
    int Ncount = 0;

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
                    if (i != next)
                    {
                        float4 r = predicted[i] - predicted[next];
                        float r_length_2 = r.x * r.x + r.y * r.y + r.z * r.z;
                        minR = min(minR, sqrt(r_length_2));

                        if (r_length_2 > 0.0f && r_length_2 < PBF_H_2)
                        {
                            Ncount++;
                            float r_length = sqrt(r_length_2);
                            float4 gradient_spiky = -1.0f * r / (r_length)
                                                    * GRAD_SPIKY_FACTOR
                                                    * (PBF_H - r_length)
                                                    * (PBF_H - r_length);

                            float poly6_r = POLY6_FACTOR * (PBF_H_2 - r_length_2) * (PBF_H_2 - r_length_2) * (PBF_H_2 - r_length_2);

                            // equation (13)
                            const float q_2 = pow(0.7f * PBF_H, 2);
                            float poly6_q = POLY6_FACTOR * (PBF_H_2 - q_2) * (PBF_H_2 - q_2) * (PBF_H_2 - q_2);
                            const float k = 0.0000005f*0;
                            const uint n = 4;

                            float s_corr = -1.0f * k * pow(poly6_r / poly6_q, n);

                            // Sum for delta p of scaling factors and grad spiky
                            // in equation (12)

                            sum += (scaling[i] + scaling[next] + s_corr) * gradient_spiky;
                        }
                    }

                    next = particles_list[next];
                }
            }
        }
    }

    // equation (12)
    float4 delta_p = sum / REST_DENSITY;

    float randDist = 0.001;
    float4 future = predicted[i] + delta_p;

    // Clamp X
         if (future.x < (SYSTEM_MIN_X + wave_generator))  future.x = SYSTEM_MIN_X + wave_generator + frand(&randSeed) * randDist;
    else if (future.x > (SYSTEM_MAX_X                 ))  future.x = SYSTEM_MAX_X                  - frand(&randSeed) * randDist;

    // Clamp Y
    if (future.y < SYSTEM_MIN_Y)      future.y = SYSTEM_MIN_Y + frand(&randSeed) * randDist;
    else if (future.y > SYSTEM_MAX_Y) future.y = SYSTEM_MAX_Y - frand(&randSeed) * randDist;

    // Clamp Z
         if (future.z < SYSTEM_MIN_Z) future.z = SYSTEM_MIN_Z + frand(&randSeed) * randDist;
    else if (future.z > SYSTEM_MAX_Z) future.z = SYSTEM_MAX_Z - frand(&randSeed) * randDist;

	// Compute delta
    delta[i] = future - predicted[i];

    // #if defined(USE_DEBUG)
    //printf("compute_delta: result: i: %d (N=%d)\ndelta: [%f,%f,%f,%f]\n",
    //      i, Ncount,
    //      delta[i].x, delta[i].y, delta[i].z, minR);
    // #endif
}
