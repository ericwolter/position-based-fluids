__kernel void computeScaling(__constant struct Parameters *Params,
                             __global float4 *predicted,
                             __global float *density,
                             const __global int *friends_list,
                             const int N)
{
    // Scaling = lambda
    const int i = get_global_id(0);
    if (i >= N) return;

    const float e = Params->epsilon * Params->restDensity;

    // Sum of rho_i, |nabla p_k C_i|^2 and nabla p_k C_i for k = i
    float density_sum = 0.0f;
    float gradient_sum_k = 0.0f;
    float3 gradient_sum_k_i = (float3) 0.0f;

    // read number of friends
    int totalFriends = 0;
    int circleParticles[FRIENDS_CIRCLES];
    for (int j = 0; j < FRIENDS_CIRCLES; j++)
        totalFriends += circleParticles[j] = friends_list[i * PARTICLE_FRIENDS_BLOCK_SIZE + j];

    int proccedFriends = 0;
    for (int iCircle = 0; iCircle < FRIENDS_CIRCLES; iCircle++)
    {
        // Check if we want to process/skip next friends circle
        if (((float)proccedFriends) / totalFriends > 0.6)
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

            const float3 r = predicted[i].xyz - predicted[j_index].xyz;
            const float r_length_2 = r.x * r.x + r.y * r.y + r.z * r.z;

            // Required for numerical stability
            if (r_length_2 > 0.0f && r_length_2 < Params->h_2)
            {
                const float r_length = sqrt(r_length_2);

                // CAUTION: the two spiky kernels are only the same
                // because the result is only used sqaured
                // equation (8), if k = i
                const float h_r_diff = Params->h - r_length;
                const float3 gradient_spiky = GRAD_SPIKY_FACTOR * h_r_diff * h_r_diff *
                                              r / r_length;

                // equation (2)
                const float h2_r2_diff = Params->h_2 - r_length_2;
                density_sum += h2_r2_diff * h2_r2_diff * h2_r2_diff;

                // equation (9), denominator, if k = j
                const float length_gradient_spiky = length(gradient_spiky);
                gradient_sum_k += length_gradient_spiky * length_gradient_spiky;

                // equation (8), if k = i
                gradient_sum_k_i += gradient_spiky;
            }
        }
    }

    // Apply Poly6 factor to density and save density
    density_sum *= POLY6_FACTOR;
    density[i] = density_sum;

    // equation (9), denominator, if k = i
    gradient_sum_k += length(gradient_sum_k_i) * length(gradient_sum_k_i);

    // equation (1)
    float density_constraint = (density_sum / Params->restDensity) - 1.0f;

    // equation (11)
    float scalingResult = -1.0f * density_constraint /
                          (gradient_sum_k / (Params->restDensity * Params->restDensity) + e);
    predicted[i].w = scalingResult;
}
