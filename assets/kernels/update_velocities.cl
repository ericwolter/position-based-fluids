__kernel void updateVelocities(__constant struct Parameters* Params, 
                               __global float4 *positions,
                               __read_only image2d_t imgPredicted,
                               __write_only image2d_t texPositions,
                               __global float4 *velocities,
                               const uint N)
{
    const uint i = get_global_id(0);
    if (i >= N) return;

    float4 newPosition = imgReadf4(imgPredicted, i);

    velocities[i].xyz = (newPosition.xyz - positions[i].xyz) / Params->timeStep;
    positions[i]      = (float4)(newPosition.xyz, fast_length(velocities[i].xyz));

    int imgWidth = get_image_width(texPositions);
    write_imagef(texPositions, (int2)(i % imgWidth, i / imgWidth), newPosition);

    // #if defined(USE_DEBUG)
    // printf("updateVelocites: i,t: %d,%f\npos: [%f,%f,%f]\npredict: [%f,%f,%f]\nvel: [%f,%f,%f]\n",
    //        i, timestep, positions[i].x, positions[i].y, positions[i].z,
    //        predicted[i].x, predicted[i].y, predicted[i].z,
    //        velocities[i].x, velocities[i].y, velocities[i].z);
    // #endif
}