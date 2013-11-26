__kernel void computeScaling(__constant struct Parameters* Params,
                             __global float4 *predicted,
                             __global float *scaling,
                             const __global int *friends_list,
                             const int N)
{
    // Scaling = lambda
    const int i = get_global_id(0);
    if (i >= N) return;

    const float e = Params->epsilon * Params->restDensity;

    // calculate current cell
    int3 current_cell = convert_int3(predicted[i].xyz * (float3)(Params->gridRes));

    // Sum of rho_i, |nabla p_k C_i|^2 and nabla p_k C_i for k = i
    float density_sum = 0.0f;
    float gradient_sum_k = 0.0f;
    float3 gradient_sum_k_i = (float3) 0.0f;
    
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
        
            float3 r = predicted[i].xyz - predicted[j_index].xyz;
            float r_length_2 = (r.x * r.x + r.y * r.y + r.z * r.z);

            // If h == r every term gets zero, so < h not <= h
            if (r_length_2 > 0.0f && r_length_2 < Params->h_2)
            {
                float r_length = sqrt(r_length_2);

                //CAUTION: the two spiky kernels are only the same
                //because the result is only used sqaured
                // equation (8), if k = i
                float3 gradient_spiky = r / (r_length)
                                        * GRAD_SPIKY_FACTOR
                                        * (Params->h - r_length)
                                        * (Params->h - r_length);

                // equation (2)
                float poly6 = POLY6_FACTOR * (Params->h_2 - r_length_2)
                              * (Params->h_2 - r_length_2)
                              * (Params->h_2 - r_length_2);
                density_sum += poly6;

                // equation (9), denominator, if k = j
                gradient_sum_k += length(gradient_spiky) * length(gradient_spiky);

                // equation (8), if k = i
                gradient_sum_k_i += gradient_spiky;
            }
        }
    }

    // equation (9), denominator, if k = i
    gradient_sum_k += length(gradient_sum_k_i) * length(gradient_sum_k_i);

    predicted[i].w = density_sum;

    // equation (1)
    float density_constraint = (density_sum / Params->restDensity) - 1.0f;

    // equation (11)
    scaling[i] = -1.0f * density_constraint / 
		                 (gradient_sum_k / (Params->restDensity * Params->restDensity) + e);
}
