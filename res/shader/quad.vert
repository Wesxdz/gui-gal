#version 400 core
layout (location = 0) in vec4 vertex;

out vec2 coords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform vec4 box = vec4(0.0, 0.0, 1.0, 1.0); // x, y offset z, w scale
uniform vec2 scale = vec2(1.0, 1.0);

void main()
{
    coords = box.xy + vec2(vertex.z * box.z * scale.x, (vertex.w * box.w * scale.y));
    gl_Position = projection * view * model * vec4(vertex.xy, 0.0, 1.0);
}