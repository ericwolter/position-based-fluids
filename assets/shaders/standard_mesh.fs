#version 150

// Inputs
in vec3 frag_vsNormal;

// outputs
out vec4 colorOut;

void main()
{
    colorOut = vec4(frag_vsNormal,1);
}
