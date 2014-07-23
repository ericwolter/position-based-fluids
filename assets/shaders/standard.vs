#version 150

uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;
uniform mat3 normalMatrix;

// input attributes
in vec4 position;
in vec3 normal;
in vec2 uv;

// Outputs to fragment shader
out vec2 frag_uv;
out vec3 frag_vsPosition; // View space position
out vec3 frag_vsNormal;

void main()
{  
    // Model -> View space 
    vec4 eye_position = modelViewMatrix * vec4(position.xyz, 1.0);

    // Send viewspace position to fragment shader
    frag_vsPosition = eye_position.xyz;
    
    // Send viewspace normal to fragment shader
    frag_vsNormal = normalMatrix * normal;

    // View -> Homogeneous space
    gl_Position = projectionMatrix * eye_position;
    
    // UV coords
    frag_uv = uv;
}

