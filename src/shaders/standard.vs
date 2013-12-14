#version 120

uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;

attribute vec4 position;
attribute vec2 uv;

varying vec2 frag_uv;
varying vec3 frag_vsPosition; // View space position

void main()
{  
    // Model -> View space 
    vec4 eye_position = modelViewMatrix * vec4(position.xyz, 1.0);

    // Send viewspace position to fragment shader
    frag_vsPosition = eye_position.xyz;

    // View -> Homogeneous space
    gl_Position = projectionMatrix * eye_position;
    
    // UV coords
    frag_uv = uv;
}

