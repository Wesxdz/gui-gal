#version 300 es
in lowp vec2 coords;
in lowp vec4 crop;
in lowp vec2 dims;
out highp vec4 color;

uniform sampler2D sprite;

void main()
{
    color = texture(sprite, coords);
    color.w = min(color.w,
    min(
        min(float(coords.x > crop.x), float(coords.y > crop.y)),
        min(float(coords.x < crop.z), float(coords.y < crop.w))
    )
    );
}