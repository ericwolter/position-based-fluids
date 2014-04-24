__kernel void packData(cbufferf_writeonly imgTarget,
                       cbufferf_readonly  imgSource,
                       __global float *packSource,
                       const uint N)
{
    const uint i = get_global_id(0);
    if (i >= N) return;

    float4 newValue = (float4)(cbufferf_read(imgSource, i).xyz, packSource[i]); 
    cbufferf_write(imgTarget, i, newValue);
}
