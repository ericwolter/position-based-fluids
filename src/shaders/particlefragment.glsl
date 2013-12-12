#version 120



uniform mat4  projectionMatrix;
uniform float pointSize;
uniform int   colorMethod;

varying float frag_color;
varying vec3  frag_vsPosition; // View space position

// const vec3 light_direction = vec3(1.0, 1.0, 1.0);
// const vec4 light_intensity = vec4(1.0, 1.0, 1.0, 1.0);

void main()
{
    // calculate normal from texture coordinates
    vec3 n;
    n.xy = gl_PointCoord.st*vec2(2.0, 2.0) + vec2(-1.0, -1.0);
 
    // discard pixels outside circle
    float mag = dot(n.xy, n.xy);
    if (mag > 1.0) discard;   
    n.z = sqrt(1.0-mag);
 
    // point on surface of sphere in eye space
    vec4 spherePosEye = vec4(frag_vsPosition + n * pointSize, 1.0);
 
    // convert to ClipSpace
    vec4 clipSpacePos = projectionMatrix * spherePosEye;
    float ndcDepth = clipSpacePos.z/clipSpacePos.w;
    
    // Clip adjusted-z
    if (ndcDepth < -1.0)
        discard;
 
    // Transform into window coordinates coordinates
    float far = gl_DepthRange.far;
    float near = gl_DepthRange.near;
    gl_FragDepth = (abs(far - near) * ndcDepth + near + far) / 2.0;

    // Compute color
    gl_FragData[0] = vec4(n, 1.0); 
}
