__kernel void updatePredicted(cbufferf_readonly imgPredictedSrc, 
                              cbufferf_writeonly imgPredictedDst, 
                              const __global float4 *delta,
                              const uint N)
{
    const uint i = get_global_id(0);
    if (i >= N) return;
    
    float4 newPredicted = cbufferf_read(imgPredictedSrc, i) + delta[i];
    cbufferf_write(imgPredictedDst, i, newPredicted);
}
