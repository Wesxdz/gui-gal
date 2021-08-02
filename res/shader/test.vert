#version 400

layout (location = 0) in vec4 vertex;

void main()
{
    gl_Position = vec4(vertex.xy, 0.0, 1.0);
}
