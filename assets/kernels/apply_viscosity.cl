__kernel void applyViscosity(
    __constant struct Parameters *Params,
    const __global float4 *predicted,
    const __global float4 *velocities,
    __global float4 *deltaVelocities,
    __global float4 *omegas,
    const __global int *friends_list,
    const int N)
{
    const int i = get_global_id(0);
    if (i >= N) return;

    float3 viscosity_sum = (float3) 0.0f;
    float3 omega_i = (float3) 0.0f;

    // read number of friends
    int totalFriends = 0;
    int circleParticles[MAX_FRIENDS_CIRCLES];
    for (int j = 0; j < MAX_FRIENDS_CIRCLES; j++)
        totalFriends += circleParticles[j] = friends_list[j * MAX_PARTICLES_COUNT + i];

    int proccedFriends = 0;
    for (int iCircle = 0; iCircle < MAX_FRIENDS_CIRCLES; iCircle++)
    {
        // Check if we want to process/skip next friends circle
        if (((float)proccedFriends) / totalFriends > 0.5f)
            continue;

        // Add next circle to process count
        proccedFriends += circleParticles[iCircle];
        
        // Compute friends start offset
        int baseIndex = FRIENDS_BLOCK_SIZE +                                      // Skip friendsCount block
                        iCircle * (MAX_PARTICLES_COUNT * MAX_FRIENDS_IN_CIRCLE) + // Offset to relevent circle
                        i;                                                        // Offset to particle_index              

        // Process friends in circle
        for (int iFriend = 0; iFriend < circleParticles[iCircle]; iFriend++)
        {
            // Read friend index from friends_list
            const int j_index = friends_list[baseIndex + iFriend * MAX_PARTICLES_COUNT];

            const float3 r = predicted[i].xyz - predicted[j_index].xyz;
            const float r_length_2 = (r.x * r.x + r.y * r.y + r.z * r.z);

            if (r_length_2 < Params->h_2)
            {
                // ignore particles where the density is zero
                // this is either a numerical issue or a problem
                // with estimating the density by sampling the neighborhood
                // In this case the standard SPH gradient operator brakes
                // because of the division by zero.
                if (fabs(predicted[j_index].w) > 1e-8f)
                {
                    const float3 v = velocities[j_index].xyz - velocities[i].xyz;
                    const float h2_r2_diff = Params->h_2 - r_length_2;

                    // equation 15
                    const float r_length = sqrt(r_length_2);
                    const float3 gradient_spiky = r / (r_length)
                                                  * (Params->h - r_length)
                                                  * (Params->h - r_length);
                    // the gradient has to be negated because it is with respect to p_j
                    omega_i += cross(v, gradient_spiky);

                    viscosity_sum += (1.0f / predicted[j_index].w) * v * (h2_r2_diff * h2_r2_diff * h2_r2_diff);
                }
            }
        }
    }

    viscosity_sum *= POLY6_FACTOR;
    deltaVelocities[i] = Params->viscosityFactor * (float4)(viscosity_sum, 0.0f);

    // save omega for later calculation of vorticity
    // cross product is compatible with scalar multiplication
    omega_i *= GRAD_SPIKY_FACTOR;
    omegas[i] = (float4)(omega_i, 0.0f);
}
