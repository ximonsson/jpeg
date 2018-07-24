#version 440

subroutine void renderer ();
subroutine uniform renderer render;

uniform sampler2D       tex;
uniform samplerBuffer   texbuf;
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

subroutine (renderer)
void render_uyvy_texbuf ()
{
    vec4 uyvy = texelFetch (texbuf, int (int (texture_coords.y * height) * width + texture_coords.x * width));
    // index of the Y component ( 1 or 3? )
    // it depends on wether this is an even pixel or not
    int i = 1 + int (mod (floor (texture_coords.x * width * 2), 2.0)) * 2;
    // get YUV data
    vec3 rgb = yuv_to_rgb (vec3 (uyvy[i], uyvy[0], uyvy[2]));
    frag_color = vec4 (rgb, color.a);
}

subroutine (renderer)
void render_texture ()
{
    frag_color = texture (tex, texture_coords) * color;
}

void main ()
{
    render ();
}
