__kernel void applyVorticity(
    __constant struct Parameters *Params,
    cbufferf_readonly imgPredicted,
    __global float4 *velocities,
    const __global float4 *omegas,
    const __global int *friends_list,
    const int N)
{
    const int i = get_global_id(0);
    if (i >= N) return;
    
    float4 particle_i = cbufferf_read(imgPredicted, i);

    float3 eta = (float3)0.0f;

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
            
            // Read particle "j" position
            const float4 particle_j = cbufferf_read(imgPredicted, j_index);
            
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
                    const float r_length = sqrt(r_length_2);
                    const float3 gradient_spiky = -1.0f * r / (r_length)
                                                  * (Params->h - r_length)
                                                  * (Params->h - r_length);

                    const float omega_length = fast_length(omegas[j_index].xyz);
                    eta += (omega_length / particle_j.w) * gradient_spiky;
                }
            }
        }
    }

    const float3 eta_N = normalize(eta * GRAD_SPIKY_FACTOR);
    const float3 vorticityForce = Params->vorticityFactor * cross(eta_N, omegas[i].xyz);
    const float3 vorticityVelocity = vorticityForce * Params->timeStep;

    velocities[i] += (float4)(vorticityVelocity, 0.0f);
}
