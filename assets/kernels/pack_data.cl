__kernel void packData(const __global float4 *packTarget,
                       __global float *packSource,
                       const uint N)
{
    const uint i = get_global_id(0);
    if (i >= N) return;

    packTarget[i].w = packSource[i];
}
