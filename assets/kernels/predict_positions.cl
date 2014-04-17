__kernel void predictPositions(__constant struct Parameters *Params,
                               const uint pauseSim,
                               const __global float4 *positions,
                               __global float4 *predicted,
                               __global float4 *velocities,
                               const uint N)
{
    const uint i = get_global_id(0);
    if (i >= N) return;

    if (pauseSim == 0)
    {
        // Append gravity (if simulation isn't pause)
        velocities[i] = velocities[i] + Params->timeStep * (float4)(0.0f, -Params->gravity, 0.0f, 0.0f);
        
        // Compute new predicted position
        predicted[i]  = positions[i]  + Params->timeStep * velocities[i];
    }
}
