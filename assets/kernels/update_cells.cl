__kernel void updateCells(__constant struct Parameters* Params, 
                          const __global float4 *predicted,
                          __global int *cells,
                          __global int *particles_list,
                          const uint N)
{
    // Get particle and assign them to a cell
    const uint i = get_global_id(0);
    if (i >= N) return;

    int3 current_cell = convert_int3(predicted[i].xyz / Params->h);
    uint cell_index = calcGridHash(current_cell);

    // Exchange cells[cell_index] and particle_list at i
    particles_list[i] = atomic_xchg(&cells[cell_index], i);

    // #if defined(USE_DEBUG)
    // printf("UPDATE_CELL: %d cell_index:%d\n", i, cell_index);
    // #endif
}
