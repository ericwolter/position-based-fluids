__kernel void computeScaling(__constant struct Parameters *Params,
                             __global float4 *predicted,
                             __global float *density,
                             const __global uint *friends_list,
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

	__private float4 particle = predicted[i];
    const float e = Params->epsilon * Params->restDensity;

    // Sum of rho_i, |nabla p_k C_i|^2 and nabla p_k C_i for k = i
    float density_sum = 0.0f;
    float gradient_sum_k = 0.0f;
    float3 gradient_sum_k_i = (float3) 0.0f;

    // Start grid scan
	uint listsCount = friends_list[i];
	for(int listIndex = 0; listIndex < listsCount; ++listIndex) {
	
		int startIndex = MAX_PARTICLES_COUNT + i * (27*2) + listIndex * 2 + 0;
		int lengthIndex = startIndex + 1;
		
		uint start = friends_list[startIndex];
		uint length = friends_list[lengthIndex];
		uint end = start + length;
		
		// iterate over all particles in this cell
		for(int j_index = start; j_index < end; ++j_index) {
			// Skip self
			if (i == j_index)
				continue;

			// Get j particle data
			const float4 j_data = predicted[j_index];
			// float4 j_data;
			// if (j_index / local_size == group_id)
			//     j_data = loc_predicted[j_index % local_size];
			// else
			//     j_data = predicted[j_index];

			const float3 r = particle.xyz - j_data.xyz;
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
    predicted[i].w = scalingResult;
}
