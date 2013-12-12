__kernel void predictPositions(__constant struct Parameters* Params, 
                               const __global float4 *positions,
                               __global float4 *predicted,
                               __global float4 *velocities,
                               const uint N)
{
    const uint i = get_global_id(0);
    if (i >= N) return;

    velocities[i].xyz = velocities[i].xyz + Params->timeStep * (float3)(0.0f, -Params->garvity, 0.0f);
    predicted[i].xyz  = positions[i].xyz  + Params->timeStep * velocities[i].xyz;
}
