#version 120

uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;

uniform float widthOfNearPlane;
uniform float pointSize;
uniform int   particleCount;
uniform int   colorMethod;

attribute vec4 position;

varying float frag_color;
varying vec3  frag_vsPosition; // View space position

void main()
{  
    // Choose color method (0=speed, 1=index)
    frag_color = colorMethod == 0 ? position.w : float(gl_VertexID) / particleCount;
    
    // Model -> View space 
    vec4 eye_position = modelViewMatrix * vec4(position.xyz, 1.0);

    // Send viewspace position to fragment shader
    frag_vsPosition = eye_position.xyz;

    // View -> Homogeneous space
    gl_Position = projectionMatrix * eye_position;

    // Compute point size
    // http://gamedev.stackexchange.com/a/54492
    gl_PointSize = widthOfNearPlane * pointSize / gl_Position.w;
}

