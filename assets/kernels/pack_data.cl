__kernel void packData(__write_only image2d_t imgTarget,
                        __read_only image2d_t imgSource,
                       __global float *packSource,
                       const uint N)
{
    const uint i = get_global_id(0);
    if (i >= N) return;

    float4 newValue = (float4)(imgReadf4(imgSource, i).xyz, packSource[i]); 
    imgWritef4(imgTarget, i, newValue);
}
