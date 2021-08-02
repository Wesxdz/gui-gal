#version 400 core
layout (location = 0) in vec4 vertex;

out vec2 coords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform vec4 box; // x, y offset z, w scale
uniform vec2 scale = vec2(1.0, 1.0);

void main()
{
    coords = box.xy + vec2(vertex.z * box.z * scale.x, (vertex.w * box.w * 1.0));
    gl_Position = vec4(vertex.xy, 0.0, 1.0); // projection * view * model *
}