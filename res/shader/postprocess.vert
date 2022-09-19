#version 300 es
layout (location = 0) in vec4 vertex;

out vec2 coords;

void main()
{
    coords = vertex.zw;
    gl_Position = vec4(vertex.xy, 0.0, 1.0);
}