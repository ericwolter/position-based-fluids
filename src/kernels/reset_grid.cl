__kernel void resetGrid(__global int *cells)
{
	cells[get_global_id(0)] = -1;
}
