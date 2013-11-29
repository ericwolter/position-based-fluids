__kernel void resetPartList(__global int *particles_list)
{
	particles_list[get_global_id(0)] = -1;
}
