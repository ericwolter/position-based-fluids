__kernel void computeScaling(__constant struct Parameters *Params,
                             cbufferf_readonly imgPredicted,
                             __global float *density,
                             __global float *lambda,
                             const __global int4 *friends_list,
                             const int N)
{
    // Scaling = lambda
    const int i = get_global_id(0);
    
    // const size_t local_size = 400;
    // const uint li = get_local_id(0);
    // const uint group_id = get_group_id(0);

    // float4 i_data;
    // if (i >= N) {
    //     i_data = (float4)(0.0f);
    // } else {
    //     i_data = predicted[i];
    // }

    // // Load data into shared block
    // __local float4 loc_predicted[local_size]; //size=local_size*4*4
    // loc_predicted[li] = i_data;
    // barrier(CLK_LOCAL_MEM_FENCE | CLK_GLOBAL_MEM_FENCE);    

    if (i >= N) return;

    // Read particle "i" position
    float3 particle_i = cbufferf_read(imgPredicted, i).xyz;
    
    // Cache parameters
    const float e = Params->epsilon;

    // Sum of rho_i, |nabla p_k C_i|^2 and nabla p_k C_i for k = i
    float density_sum = 0.0f;
    float gradient_sum_k = 0.0f;
    float3 gradient_sum_k_i = (float3) 0.0f;

    for (int o = 0; o < 9; ++o)
    {
        int3 neighborCells = friends_list[i + N * o].xyz;

        for(int d = 0; d < 3; ++d)
        {
            int data = neighborCells[d];
            if (data == END_OF_CELL_LIST) continue;
            int entries = data >> 24;
            data = data & 0xFFFFFF;

            // if(i==0) {
            //     printf("scaling: %d, %d,%d: (%d,%d)\n",i,o,d,data,entries);
            // }

            for(int j_index = data; j_index < data + entries; ++j_index)
            {
                if(j_index == i) continue;

                // Get j particle data
                const float3 position_j = cbufferf_read(imgPredicted, j_index).xyz;

                const float3 r = particle_i - position_j;
                const float r_length_2 = dot(r,r);

                // Required for numerical stability
                if (r_length_2 < Params->h_2)
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
                    gradient_sum_k += dot(gradient_spiky, gradient_spiky);

                    // equation (8), if k = i
                    gradient_sum_k_i += gradient_spiky;
                }                
            }
        }
    }

    // Apply Poly6 factor to density and save density
    density_sum *= POLY6_FACTOR;
    density[i] = density_sum;

    // equation (9), denominator, if k = i
    gradient_sum_k += dot(gradient_sum_k_i, gradient_sum_k_i);

    // equation (1)
    float density_constraint = (density_sum / Params->restDensity) - 1.0f;

    // equation (11)
    float scalingResult = -1.0f * density_constraint /
                          (gradient_sum_k / (Params->restDensity * Params->restDensity) + e);
                          
    lambda[i] = scalingResult;
}
