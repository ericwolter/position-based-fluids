__kernel void computeScaling(__constant struct Parameters* Params,
                             __global float4 *predicted,
                             __global float *scaling,
                             const __global int *cells,
                             const __global int *particles_list,
                             const int N)
{
    // Scaling = lambda
    const int i = get_global_id(0);
    if (i >= N) return;

    const int END_OF_CELL_LIST = -1;
    const float e = Params->epsilon * Params->restDensity;

    // calculate current cell
    int3 current_cell = convert_int3(predicted[i].xyz * (float3)(Params->gridRes));

    // Sum of rho_i, |nabla p_k C_i|^2 and nabla p_k C_i for k = i
    float density_sum = 0.0f;
    float gradient_sum_k = 0.0f;
    float3 gradient_sum_k_i = (float3) 0.0f;

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
                        float3 r = predicted[i].xyz - predicted[next].xyz;
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

                    next = particles_list[next];
                }
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
