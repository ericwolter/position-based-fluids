#version 120
#extension GL_EXT_gpu_shader4 : require

uniform mat4 cameraToClipMatrix;
uniform mat4 worldToCameraMatrix;
uniform mat4 modelToWorldMatrix;

uniform float particleCount;

attribute vec4 position;

varying float frag_velocity;

void main()
{  
    frag_velocity = gl_VertexID / particleCount;
    
    vec4 eye_position = worldToCameraMatrix * modelToWorldMatrix * vec4(position.xyz, 1.0);
    gl_Position = cameraToClipMatrix * eye_position;

    float a = 1.0;
    float b = 0.0;
    float c = 10.0;
    float d = length(position.xyz);
    float size = 10.0;
    float derived_size = size * sqrt(1.0/(a + b * d + c * d * d));

    gl_PointSize = 18.0 / gl_Position.w;
}

