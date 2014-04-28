__kernel void applyViscosity(
    __constant struct Parameters *Params,
    cbufferf_readonly imgPredicted,
    __global float4 *velocities,
    __global float4 *omegas,
    const __global int *friends_list,
    const int N)
{
    const int i = get_global_id(0);
    if (i >= N) return;

    float4 particle_i = cbufferf_read(imgPredicted, i);
    float4 velocity_i = velocities[i];

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

            float4 particle_j = cbufferf_read(imgPredicted, j_index);
            float4 velocity_j = velocities[j_index];

            const float3 r = particle_i.xyz - particle_j.xyz;
            const float r_length_2 = (r.x * r.x + r.y * r.y + r.z * r.z);

            if (r_length_2 < Params->h_2)
            {
                // ignore particles where the density is zero
                // this is either a numerical issue or a problem
                // with estimating the density by sampling the neighborhood
                // In this case the standard SPH gradient operator brakes
                // because of the division by zero.
                if (fabs(particle_j.w) > 1e-8f)
                {
                    const float3 v = velocity_j.xyz - velocity_i.xyz;
                    const float h2_r2_diff = Params->h_2 - r_length_2;

                    // equation 15
                    const float r_length = sqrt(r_length_2);
                    const float3 gradient_spiky = r / (r_length)
                                                  * (Params->h - r_length)
                                                  * (Params->h - r_length);
                    // the gradient has to be negated because it is with respect to p_j
                    omega_i += cross(v, gradient_spiky);
                    
                    // the original ghost sph paper scales the term with the neighbors density
                    // however formula in the PBF paper omits this term which seems incorrect...
                    //viscosity_sum += (1.0f / particle_j.w) * v * (h2_r2_diff * h2_r2_diff * h2_r2_diff);
                    viscosity_sum += v * (h2_r2_diff * h2_r2_diff * h2_r2_diff);
                }
            }
        }
    }

    viscosity_sum *= POLY6_FACTOR;
    velocities[i] += Params->viscosityFactor * (float4)(viscosity_sum, 0.0f);

    // save omega for later calculation of vorticity
    // cross product is compatible with scalar multiplication
    omega_i *= GRAD_SPIKY_FACTOR;
    omegas[i] = (float4)(omega_i, 0.0f);
}
