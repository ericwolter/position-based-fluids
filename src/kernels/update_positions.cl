__kernel void updatePositions(__global float4 *positions,
                              const __global float4 *predicted,
                              __write_only image2d_t texPositions,
                              __global float4 *velocities,
                              const __global float4 *deltaVelocities,
                              const uint N)
{
    const uint i = get_global_id(0);
    if (i >= N) return;

    float4 newPos = predicted[i];
    positions[i].xyz = newPos.xyz;
    
    // Update position texture
    int imgWidth = get_image_width(texPositions);
    write_imagef(texPositions, (int2)(i % imgWidth, i / imgWidth), newPos);

    // Ugly hack to cirumvent particles resetting if sorting is done in the beginning
    float3 dV = deltaVelocities[i].xyz;
    float l_dV = fast_length(dV);
    if(l_dV > 1.0f) {
        dV = normalize(dV);
    }
    
    velocities[i].xyz += dV;

    positions[i].w = length(velocities[i].xyz);

    // #if defined(USE_DEBUG)
    // printf("%d: pos:[%f,%f,%f]\nvel: [%f,%f,%f]\n", i,
    //        positions[i].x, positions[i].y, positions[i].z,
    //        velocities[i].x, velocities[i].y, velocities[i].z
    //       );
    // #endif
}
