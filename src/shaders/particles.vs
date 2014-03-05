#version 150

uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;

uniform float widthOfNearPlane;
uniform float pointSize;
uniform int   particleCount;

// attributes
in vec4 position;

// Outputs to fragment shader
out float      frag_color;
out vec3       frag_vsPosition; // View space position
flat out int   frag_particleIndex;
flat out float frag_velocity;

void main()
{  
    // Store particle index
    frag_particleIndex = gl_VertexID; 
 
    // Forward velocity to fragment shader
    frag_velocity = position.w;
    
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

