int expandBits1(int x)
{
    x = (x | (x << 16)) & 0x030000FF;
    x = (x | (x <<  8)) & 0x0300F00F;
    x = (x | (x <<  4)) & 0x030C30C3;
    x = (x | (x <<  2)) & 0x09249249;

    return x;
}

uint calcGridHash(int3 gridPos)
{
    return (expandBits1(gridPos.x) | (expandBits1(gridPos.y) << 1) | (expandBits1(gridPos.z) << 2)) % GRID_BUG_SIZE;
}

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
