__kernel void updatePredicted(__read_only image2d_t imgPredictedSrc, 
                              __write_only image2d_t imgPredictedDst, 
                              const __global float4 *delta,
                              const uint N)
{
    const uint i = get_global_id(0);
    if (i >= N) return;
    
    float4 newPredicted = imgReadf4(imgPredictedSrc, i) + delta[i];
    imgWritef4(imgPredictedDst, i, newPredicted);
}
