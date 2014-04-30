float3 BouncePointQuad(float3 PrevPos, float3 NextPos, float3 B, float3 E0, float3 E1, float EdgeOffset)
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
    if ((s < 0) || (s > 1) || (t < 0) || (t > 1))
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
        
    // Move point surface
    return planePos;
}

__kernel void computeDelta(__constant struct Parameters *Params,
                           volatile __global int *debugBuf,
                           __global float4 *delta,
                           const __global float4 *positions,
                           cbufferf_readonly imgPredicted, // xyz=predicted, w=scaling
                           const __global int4 *friends_list,
                           const float wave_generator,
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

    for (int o = 0; o < 9; ++o)
    {
        int3 neighborCells = friends_list[i + N * o].xyz;

        for(int d = 0; d < 3; ++d)
        {
            int data = neighborCells[d];
            int entries = data >> 24;
            data = data & 0xFFFFFF;

            if (data == END_OF_CELL_LIST) continue;

            for(int j_index = data; j_index < data +entries; ++j_index)
            {
                if(j_index == i) continue;

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
    }

    // equation (12)
    float3 delta_p = (-GRAD_SPIKY_FACTOR*sum) / Params->restDensity;
    float3 future = particle_i.xyz + delta_p;

    // Compute edge offset
    float3 noisePos = future * 5 / Params->h;
    float edgeOffset = (3 + sin(noisePos.x)+sin(noisePos.y)+sin(noisePos.z)) * Params->h * 0.03f;

    // Clamp Y
    future.y = max(future.y, Params->yMin + edgeOffset);
    future.z = clamp(future.z, Params->zMin + edgeOffset, Params->zMax + edgeOffset);
    future.x = clamp(future.x, Params->xMin + edgeOffset + wave_generator, Params->xMax + edgeOffset);
    
    //future = BouncePointQuad(positions[i].xyz, future, (float3)(  0,  0,  0), (float3)(0, 0, 1.1), (float3)(4.1, 0, 0), edgeOffset);
    //future = BouncePointQuad(positions[i].xyz, future,   (float3)(-40,  0,-40), (float3)(0, 0, 80), (float3)(60, 0, 0), edgeOffset);
    //future = BouncePointQuad(positions[i].xyz, future,   (float3)(  0,-40,-40), (float3)(0, 0, 80), (float3)(60, 0, 0), edgeOffset);

    // Compute delta
    delta[i].xyz = future - particle_i.xyz;
}
