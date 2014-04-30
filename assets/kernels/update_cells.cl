__kernel void updateCells(__constant struct Parameters* Params,
						  const __global int *keys,
						  __global int *cells,
                         const uint N)
{
    const uint i = get_global_id(0);
    if (i >= N) return;
	
	int cell_index = keys[i];
	
	if (i > 0) {
		int prev_cell_index = keys[i - 1];
		// compare previous cell in sorted
		if(prev_cell_index != cell_index) {
			cells[cell_index*2+0] = i;
			cells[prev_cell_index*2+1] = i - 1;
		}
		// the last particle is ALWAYS the end of a cell
		if (i == (N-1)) {
			cells[cell_index*2+1] = i;
		}
	// the first particle is ALWAYS the start of a cell
	} else if (i == 0) {
		cells[cell_index*2+0] = i;
	} 
}
