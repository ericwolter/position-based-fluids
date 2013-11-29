__kernel void packData(const __global float4 *packTarget,
                             __global float *packSource)
{
    packTarget[get_global_id(0)].w = packSource[get_global_id(0)];
}
