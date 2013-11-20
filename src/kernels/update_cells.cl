uint calcGridHash(int3 gridPos)
{
    const uint p1 = 73856093; // some large primes
    const uint p2 = 19349663;
    const uint p3 = 83492791;
    return abs(p1*gridPos.x ^ p2*gridPos.y ^ p3*gridPos.z) % GRID_SIZE;
}

__kernel void updateCells(const __global float4 *predicted,
                          __global int *cells,
                          __global int *particles_list,
                          const uint N)
{
    // Get particle and assign them to a cell
    const uint i = get_global_id(0);
    if (i >= N) return;

    int3 current_cell = convert_int3(predicted[i].xyz * (float3)(GRID_RES));

    uint cell_index = calcGridHash(current_cell);

    // Exchange cells[cell_index] and particle_list at i
    particles_list[i] = atomic_xchg(&cells[cell_index], i);

    // #if defined(USE_DEBUG)
    // printf("UPDATE_CELL: %d cell_index:%d\n", i, cell_index);
    // #endif
}
