static float XorSig( float sig0, float sig1, const float zeroSig )
{
    const uint bits0     = *( ( uint *) &sig0 );
    const uint bits1     = *( ( uint *) &sig1 );
    const uint zeroBits  = *( ( uint *) &zeroSig );
    const uint resBits   = ( bits0 ^ bits1 ) | zeroBits;

    return *( ( float *) &resBits );
}

int GetMsdb( float x, float y )
{
    // Numbers are equal
    if ( x == y )
    {
        return -2147483648;//std::numeric_limits< int >::min();
    }

    // Numbers are unequal

    int xExp;
    int yExp;

    float xSig = frexp( x, &xExp );
    float ySig = frexp( y, &yExp );

    // Exponent(x) is greater
    if ( xExp > yExp )
    {
        return xExp;
    }

    // Exponent(y) is greater
    if ( yExp > xExp )
    {
        return yExp;
    }

    ////
    // Exponents of x and y are equal
    ////

    const float ZeroSig  = 0.5;
    const float resSig   = XorSig( xSig, ySig, ZeroSig );

    int resExp;
    frexp( resSig - ZeroSig, &resExp );

    return xExp + resExp;   // Add to one of the exponents (both are equal anyway)
}

bool compareZ(const float4 p, const float4 q)
{
    float *pa = &p;
    float *qa = &q;

    // printf("compareZ: \np: [%f,%f,%f]\npa:[%f,%f,%f]\n",p.x, p.y,p.z, pa[0], pa[1], pa[2]);


    int maxDim  = 0;
    int maxExp  = -2147483648;//-std::numeric_limits< int >::max();

    // Iterate coordinates
    for ( int d = 0; d < 3; ++d )
    {
        ////
        // Check if either floating point number
        // has different sign bit.
        ////

        const bool isEitherNumNeg = ( ( pa[d] < 0 ) != ( qa[d] < 0 ) );

        if ( isEitherNumNeg )
        {
            return pa[d] < qa[d];
        }

        ////
        // Both numbers have same sign bit
        ////

        const int exp = GetMsdb( pa[d], qa[d] );

        if ( maxExp < exp )
        {
            maxDim = d;
            maxExp = exp;
        }
    }

    return pa[ maxDim ] < qa[ maxDim ];
}

__kernel void sort(__global float4 *positions,
                   __global float4 *velocities,
                   const uint bucketSize,
                   const uint bucketOffset,
                   const uint particleCount,
                   const uint N)
{
    const uint segment = get_global_id(0);
    if (segment >= N) return;

    uint start = segment * bucketSize + bucketOffset;
    uint end = start + bucketSize;
    end = min(end, particleCount);

    for (uint i = start + 1; i < end; ++i)
    {
        float4 insertPos = positions[i];
        float4 insertVel = velocities[i];
        uint hole_pos = i;
        // if(segment==0){
        //     printf("segment: %d start:%d, end:%d\ni:%d hole:%d\n>: %d cmp:%d\n", segment,start,end,i,hole_pos,hole_pos>start,compareZ(insertPos, positions[hole_pos - 1]));
        // }

        while (hole_pos > start && compareZ(insertPos, positions[hole_pos - 1]))
        {
            positions[hole_pos] = positions[hole_pos - 1];
            velocities[hole_pos] = velocities[hole_pos - 1];
            hole_pos = hole_pos - 1;
        }

        positions[hole_pos] = insertPos;
        velocities[hole_pos] = insertVel;
    }
}
