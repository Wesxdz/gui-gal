#version 300 es
layout (location = 0) in vec4 vertex;

out highp vec2 coords;
out highp vec4 crop;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec4 subimage;

uniform vec4 box; // = vec4(0.0, 0.0, 1.0, 1.0); // x, y offset z, w scale
uniform vec2 scale;// = vec2(1.0, 1.0);

void main()
{
    coords = box.xy + vec2(vertex.z * box.z * scale.x, (vertex.w * box.w * scale.y));
    crop = subimage;
    gl_Position = projection * view * model * vec4(vertex.xy, 0.0, 1.0);
}