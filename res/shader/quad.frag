#version 400 core
in vec2 coords;
out vec4 color;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform sampler2D sprite;

void main()
{
    color = texture(sprite, coords);
    // color = vec4(1.0, 0.0, 0.0, 1.0);
}