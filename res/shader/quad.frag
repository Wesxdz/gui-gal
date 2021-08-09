#version 300 es
in lowp vec2 coords;
out highp vec4 color;

uniform sampler2D sprite;

void main()
{
    color = texture(sprite, coords);
}