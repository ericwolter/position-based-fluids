__kernel void applyViscosity(const __global float4 *predicted,
        const __global float4 *velocities,
        __global float4 *deltaVelocities,
        __global float4 *omegas,
        const __global int *cells,
        const __global int *particles_list,
        const int N)
{
    const int i = get_global_id(0);
    if (i >= N) return;

    const int END_OF_CELL_LIST = -1;

    int3 current_cell = convert_int3(predicted[i].xyz * (float3)(GRID_RES));

    float3 viscosity_sum = (float3) 0.0f;
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
                        float3 r = predicted[i].xyz - predicted[next].xyz;
                        float r_length_2 = (r.x * r.x + r.y * r.y + r.z * r.z);

                        if (r_length_2 > 0.0f && r_length_2 < PBF_H_2)
                        {
                            // ignore particles where the density is zero
                            // this is either a numerical issue or a problem
                            // with estimating the density by sampling the neighborhood
                            // In this case the standard SPH gradient operator brakes
                            // because of the division by zero.
                            if(fabs(predicted[next].w) > 1e-8)
                            {
                                float3 v = velocities[next].xyz - velocities[i].xyz;
                                float poly6 = POLY6_FACTOR * (PBF_H_2 - r_length_2)
                                              * (PBF_H_2 - r_length_2)
                                              * (PBF_H_2 - r_length_2);

                                // equation 15
                                float r_length = sqrt(r_length_2);
                                float3 gradient_spiky = -1.0f * r / (r_length)
                                                        * GRAD_SPIKY_FACTOR
                                                        * (PBF_H - r_length)
                                                        * (PBF_H - r_length);
                                // the gradient has to be negated because it is with respect to p_j
                                // this could be done directly when calculating it, but for now we explicitly
                                // keep it to improve understanding
                                omega_i += cross(v.xyz,-gradient_spiky.xyz);

                                viscosity_sum += (1.0f / predicted[next].w) * v * poly6;
                            }
                        }
                    }

                    next = particles_list[next];
                }
            }
        }
    }

    const float c = 0.3f;
    deltaVelocities[i] = c * (float4)(viscosity_sum.x,viscosity_sum.y,viscosity_sum.z,0.0f);

    // save omega for later calculation of vorticity
    omegas[i] = (float4)(omega_i.x, omega_i.y, omega_i.z, 0.0f);
}
