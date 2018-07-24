#version 300 es
precision highp float;

uniform sampler2D       texbuf;
uniform int             width;
uniform int             height;

in vec2 texture_coords;
in vec4 color;

layout (location = 0) out vec4 frag_color;

/**
 *  YUV to RGB
 */
vec3 yuv_to_rgb (vec3 yuv)
{
    float r, g, b, y, u, v;

    y = 1.1643 * yuv[0] - 0.0625;
	u =          yuv[1] - 0.5;
	v =          yuv[2] - 0.5;

    // convert to RGB
	r = y + 1.596 * v;
	g = y - 0.813 * v - 0.391 * u;
	b = y             + 2.018 * u;

	return vec3 (r, g, b);
}

void main ()
{
    vec4 uyvy = texture (texbuf, texture_coords);
	vec3 rgb = yuv_to_rgb (vec3 (uyvy[1], uyvy[0], uyvy[2]));
    frag_color = vec4 (rgb, color.a);
}
