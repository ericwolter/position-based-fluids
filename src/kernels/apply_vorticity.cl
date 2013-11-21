__kernel void applyVorticity(const __global float4 *predicted,
        __global float4 *deltaVelocities,
        const __global float4 *omegas,
        const __global int *cells,
        const __global int *particles_list,
        const int N)
{
    const int i = get_global_id(0);
    if (i >= N) return;

    const int END_OF_CELL_LIST = -1;

    int3 current_cell = convert_int3(predicted[i].xyz * (float3)(GRID_RES));

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

                        if (r_length_2 > 0.0f && r_length_2 < PBF_H_2)
                        {
                            float r_length = sqrt(r_length_2);
                            float4 gradient_spiky = -1.0f * r / (r_length)
                                                    * GRAD_SPIKY_FACTOR
                                                    * (PBF_H - r_length)
                                                    * (PBF_H - r_length);

                            float omega_length = length(omegas[next].xyz);
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
    float3 vorticityForce = epsilon * cross(eta_N, omegas[i].xyz);
    float3 vorticityVelocity = vorticityForce * TIMESTEP;

    deltaVelocities[i] += (float4)(vorticityVelocity.x, vorticityVelocity.y, vorticityVelocity.z, 0.0f);
}
