__kernel void resetGrid(__constant struct Parameters *Params,
                          const __global int *keys,
                        __global uint2 *cells,
                        const int N)
{
    const int i = get_global_id(0);
    if (i >= N) return;
	
    const uint cell_index = keys[i];
	
	// only reset start index for performance reasons
	// later this can be used to check and skip empty cells;
	cells[cell_index].x = END_OF_CELL_LIST;
	cells[cell_index].y = END_OF_CELL_LIST;
}
