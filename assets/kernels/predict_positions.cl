__kernel void predictPositions(__constant struct Parameters *Params,
                               const uint pauseSim,
                               const __global float4 *positions,
                               __write_only image2d_t imgPredicted,
                               __global float4 *velocities,
                               const uint N)
{
    const uint i = get_global_id(0);
    if (i >= N) return;

    // Append gravity (if simulation isn't pause)
    if (pauseSim == 0)
        velocities[i].xyz = velocities[i].xyz + Params->timeStep * (float3)(0.0f, -Params->garvity, 0.0f);
        
    // Compute new predicted position
    // predicted[i].xyz  = positions[i].xyz  + Params->timeStep * velocities[i].xyz;
    imgWritef4(imgPredicted, i,  positions[i] + Params->timeStep * velocities[i]);
}
