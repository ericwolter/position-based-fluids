#version 150

uniform mat4  projectionMatrix;
uniform float pointSize;
uniform int   colorMethod;

// inputs from vertex shader
in float frag_color;
in vec3  frag_vsPosition; // View space position

// outputs
out vec4 colorOut;

vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

vec3 colorscale(float power)
{
    float H = power * 0.7; // Hue (note 0.4 = Green, see huge chart below)
    float S = 0.9; // Saturation
    float B = 0.9; // Brightness

    return hsv2rgb(vec3(H,S,B));
}

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
    const vec3 light_direction = vec3(1.0);
    float cosAngIncidence = dot(n, light_direction);
    cosAngIncidence = clamp(cosAngIncidence, 0.0, 1.0);

    // Set color
    vec3 dif_color = colorMethod == 0 ? vec3(frag_color, frag_color, 1.0) : colorscale(frag_color);
    colorOut = vec4(dif_color, 1.0) * cosAngIncidence;    
}
