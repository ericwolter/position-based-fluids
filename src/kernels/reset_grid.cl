__kernel void resetGrid(__constant struct Parameters *Params,
                        const __global float4 *predicted,
                        __global int *particles_list,
                        __global int *cells,
                        const int N)
{
    const int i = get_global_id(0);
    if (i >= N) return;

    int3 current_cell = convert_int3(predicted[i].xyz / Params->h);
    uint cell_index = calcGridHash(current_cell);

    particles_list[i] = END_OF_CELL_LIST;
    cells[cell_index] = END_OF_CELL_LIST;
}
