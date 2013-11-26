// taken from https://bitbucket.org/ashwin/z-order-sort/src/179c9aab142158837c7283f710aad4556f4fc398/Z-Order-Sort/Main.cpp?at=default

////////////////////////////////////////////////////////////////////////////////
// Sort points on Z-Order curve.
// Handles floating point coordinates.
//
// Adapted from STANN <https://sites.google.com/a/compgeom.com/stann/>
////////////////////////////////////////////////////////////////////////////////

#include <algorithm>
#include <cstdint>
#include <vector>
#include <cmath>
#include <iostream>
using namespace std;

// Dimension of point coordinates
const int Dim = 2;

struct Point
{
    int _p[ Dim ];

    friend std::ostream& operator << (std::ostream& stream, const Point &p)
    {
        stream << "(" <<p._p[0] << "|" <<  p._p[1] << "), ";
        return stream;
    } 
};

typedef vector< Point > PointVec;

////////////////////////////////////////////////////////////////////////////////

// Compute bitwise XOR of significand of two floating point numbers.
// A baseline value is used to restore the floating point structure
// after XOR (0.5 is used here)

float XorSig( float sig0, float sig1, const float zeroSig )
{
    const uint32_t bits0     = *( ( uint32_t* ) &sig0 );
    const uint32_t bits1     = *( ( uint32_t* ) &sig1 );
    const uint32_t zeroBits  = *( ( uint32_t* ) &zeroSig );
    const uint32_t resBits   = ( bits0 ^ bits1 ) | zeroBits;

    return *( ( float* ) &resBits );
}

// Compute the most significant differing bit of two floating point numbers.
// The return value is the exponent of the highest order bit that differs
// between the two numbers.
// Note: The MSDB of two floating point numbers is NOT computed based
// on the internal representation of the floating point numbers. The answer
// is computed based on a theoretical integer representation of the numbers.

int GetMsdb( float x, float y )
{
    // Numbers are equal
    if ( x == y )
    {
        return std::numeric_limits< int >::min();
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

bool ZOrderLessThan( const Point& p, const Point& q )
{
    int maxDim  = 0;
    int maxExp  = -std::numeric_limits< int >::max();

    // Iterate coordinates
    for ( int d = 0; d < Dim; ++d )
    {
        ////
        // Check if either floating point number
        // has different sign bit.
        ////

        const bool isEitherNumNeg = ( ( p._p[d] < 0 ) != ( q._p[d] < 0 ) );

        if ( isEitherNumNeg )
        {
            return p._p[d] < q._p[d];
        }

        ////
        // Both numbers have same sign bit
        ////

        const int exp = GetMsdb( p._p[d], q._p[d] );

        if ( maxExp < exp )
        {
            maxDim = d;
            maxExp = exp;
        }
    }

    return p._p[ maxDim ] < q._p[ maxDim ];
}

// uint64_t JoinBits(uint32_t a, uint32_t b) {
//   uint64_t result = 0;
//   for(int16_t ii = 31; ii >= 0; ii--){
//     result |= (a >> ii) & 1;
//     result <<= 1;
//     result |= (b >> ii) & 1;
//     if(ii != 0){
//       result <<= 1;
//     }
//   }
//   return result;
// }

uint64_t JoinBits(uint32_t a, uint32_t b)
{
    uint64_t r = 0;

    for (uint16_t i = 0; i < 32; i++)
        r |= ((a & (1 << i)) << i) | ((b & (1 << i)) << (i + 1));

    return r;
}

bool test(const Point &p, const Point& q) 
{
    const uint32_t bits0x     = *( ( uint32_t* ) &(p._p[0]) );
    const uint32_t bits0y     = *( ( uint32_t* ) &(p._p[1]) );
    const uint32_t bits1x     = *( ( uint32_t* ) &(q._p[0]) );
    const uint32_t bits1y     = *( ( uint32_t* ) &(q._p[1]) );

    const uint64_t z0 = JoinBits(bits0x, bits0y);
    const uint64_t z1 = JoinBits(bits1x, bits1y);

    return z0 < z1;
}

////////////////////////////////////////////////////////////////////////////////
Point makePoint(float x, float y, float z) {
    Point p;
    p._p[0] = (int)x;
    p._p[1] = (int)y;
    return p;
}

void printVector(PointVec vector) {
    std::copy( vector.begin(), vector.end(), std::ostream_iterator< Point >( std::cout ) );
    cout << endl;
}

int main()
{
    PointVec pointVec;

    // Fill vector with points here ...
    for (int i = 0; i < 8; ++i)
    {
        for (int j = 0; j < 8; ++j)
        {
            pointVec.push_back(makePoint(j, i, 0.0f));
        }
    }

    cout << "Unsorted:" << endl;
    printVector(pointVec);

    // Sort points on Z Order curve
    std::sort( pointVec.begin(), pointVec.end(), test );

    cout << "Sorted:" << endl;
    printVector(pointVec);

    return 0;
}

////////////////////////////////////////////////////////////////////////////////