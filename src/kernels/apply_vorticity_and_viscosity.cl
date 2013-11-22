__kernel void applyVorticityAndViscosity(
        __constant struct Parameters* Params, 
        const __global float4 *predicted,
        const __global float4 *velocities,
        __global float4 *deltaVelocities,
        const __global int *cells,
        const __global int *particles_list,
        const int N)
{
    const int i = get_global_id(0);
    if (i >= N) return;

    const int END_OF_CELL_LIST = -1;

    int3 current_cell = convert_int3(predicted[i].xyz * (float3)(Params->gridRes));

    float4 viscosity_sum = (float4) 0.0f;
    float3 omega_i = (float3) 0.0f;

    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            for (int z = -1; z <= 1; ++z)
            {
                uint cell_index = calcGridHash(current_cell + (int3)(x,y,z));

                int next = cells[cell_index];
                while (next != END_OF_CELL_LIST)
                {
                    if (i != next)
                    {
                        float4 r = predicted[i] - predicted[next];
                        float r_length_2 = (r.x * r.x + r.y * r.y + r.z * r.z);

                        if (r_length_2 > 0.0f && r_length_2 < Params->h_2)
                        {
                            float4 v = velocities[next] - velocities[i];
                            float poly6 = POLY6_FACTOR * (Params->h_2 - r_length_2)
                                          * (Params->h_2 - r_length_2)
                                          * (Params->h_2 - r_length_2);

                            // equation 15
                            float r_length = sqrt(r_length_2);
                            float4 gradient_spiky = -1.0f * r / (r_length)
                                                    * GRAD_SPIKY_FACTOR
                                                    * (Params->h - r_length)
                                                    * (Params->h - r_length);
                            // the gradient has to be negated because it is with respect to p_j
                            // this could be done directly when calculating it, but for now we explicitly
                            // keep it to improve understanding
                            omega_i += cross(v.xyz,-gradient_spiky.xyz);

                            viscosity_sum += (1.0f / predicted[next].w) * v * poly6;

                            // #if defined(USE_DEBUG)
                            // printf("viscosity: i,j: %d,%d result: [%f,%f,%f] density: %f\n", i, next,
                            //        v.x, v.y, v.z, predicted[j].w);
                            // #endif // USE_DEBUG
                        }
                    }

                    next = particles_list[next];
                }
            }
        }
    }

    const float c = 0.3f;
    deltaVelocities[i] = c * viscosity_sum;

    // vorticity
    float omega_length = length(omega_i);
    float3 eta = (float3)0.0f;

    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            for (int z = -1; z <= 1; ++z)
            {
                uint cell_index = calcGridHash(current_cell + (int3)(x,y,z));

                int next = cells[cell_index];
                while (next != END_OF_CELL_LIST)
                {
                    if (i != next)
                    {
                        float4 r = predicted[i] - predicted[next];
                        float r_length_2 = (r.x * r.x + r.y * r.y + r.z * r.z);

                        if (r_length_2 > 0.0f && r_length_2 < Params->h_2)
                        {
                            float r_length = sqrt(r_length_2);
                            float4 gradient_spiky = -1.0f * r / (r_length)
                                                    * GRAD_SPIKY_FACTOR
                                                    * (Params->h - r_length)
                                                    * (Params->h - r_length);

                            eta += (omega_length / predicted[next].w) * gradient_spiky.xyz;
                        }
                    }

                    next = particles_list[next];
                }
            }
        }
    }

    float3 eta_N = normalize(eta);
    const float epsilon = 0.000005f;
    float3 vorticityForce = epsilon * cross(eta_N, omega_i);
    float3 vorticityVelocity = vorticityForce * Params->timeStep;
    // if(i==0) {
    //     printf("VORTICITY: %d velocity:[%f,%f,%f]\n", i, vorticityVelocity.x, vorticityVelocity.y, vorticityVelocity.z);
    // }
    deltaVelocities[i] += (float4)(vorticityVelocity.x, vorticityVelocity.y, vorticityVelocity.z, 0.0f);

    // #if defined(USE_DEBUG)
    // printf("viscosity: i: %d sum:%f result: [%f,%f,%f]\n", i,
    //        c * viscosity_sum.x, c * viscosity_sum.y, c * viscosity_sum.z);
    // #endif // USE_DEBUG
}
