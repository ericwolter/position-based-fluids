uint rand(uint2 *state)
{
    enum { A = 4294883355U};
    uint x = (*state).x, c = (*state).y; // Unpack the state
    uint res = x ^ c;                 // Calculate the result
    uint hi = mul_hi(x, A);           // Step the RNG
    x = x * A + c;
    c = hi + (x < c);
    *state = (uint2)(x, c);           // Pack the state back up
    return res;                       // Return the next result
}

float frand(uint2 *state)
{
    return rand(state) / 4294967295.0f;
}

float frand3(float3 co)
{
    float ipr;
    return fract(sin(dot(co.xy, (float2)(12.9898, 78.233))) * (43758.5453 + co.z), &ipr);
}

float rand_3d(float3 pos)
{
    float3 pos_i;
    float3 pos_f = fract(pos, &pos_i);

    // Calculate noise contributions from each of the eight corners
    float n000 = frand3(pos_i + (float3)(0, 0, 0));
    float n100 = frand3(pos_i + (float3)(1, 0, 0));
    float n010 = frand3(pos_i + (float3)(0, 1, 0));
    float n110 = frand3(pos_i + (float3)(1, 1, 0));
    float n001 = frand3(pos_i + (float3)(0, 0, 1));
    float n101 = frand3(pos_i + (float3)(1, 0, 1));
    float n011 = frand3(pos_i + (float3)(0, 1, 1));
    float n111 = frand3(pos_i + (float3)(1, 1, 1));

    // Compute the fade curve value for each of x, y, z
    float u = smoothstep(0.0f, 1.0f, pos_f.x);
    float v = smoothstep(0.0f, 1.0f, pos_f.y);
    float w = smoothstep(0.0f, 1.0f, pos_f.z);

    // Interpolate along x the contributions from each of the corners
    float nx00 = mix(n000, n100, u);
    float nx01 = mix(n001, n101, u);
    float nx10 = mix(n010, n110, u);
    float nx11 = mix(n011, n111, u);

    // Interpolate the four results along y
    float nxy0 = mix(nx00, nx10, v);
    float nxy1 = mix(nx01, nx11, v);

    // Interpolate the two last results along z
    float nxyz = mix(nxy0, nxy1, w);

    return nxyz;
}

int expandBits(int x)
{
    x = (x | (x << 16)) & 0x030000FF;
    x = (x | (x <<  8)) & 0x0300F00F;
    x = (x | (x <<  4)) & 0x030C30C3;
    x = (x | (x <<  2)) & 0x09249249;

    return x;
}

int mortonNumber(int3 gridPos)
{
    return expandBits(gridPos.x) | (expandBits(gridPos.y) << 1) | (expandBits(gridPos.z) << 2);
}

uint calcGridHash(int3 gridPos)
{
    return mortonNumber(gridPos) % GRID_BUG_SIZE;
}
