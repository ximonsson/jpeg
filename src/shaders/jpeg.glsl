/**
 *  File: jpeg.glsl
 *  Description:
 *      GLSL Compute Shader for performing JPEG decompression (i.e. iDCT) on decoded blocks
 *      supplied by Shader Storage Object, which contain the results.
 */
#version 440
#extension GL_NV_gpu_shader5 : enable

layout (local_size_x = 8, local_size_y = 8) in;

layout (binding = 4) buffer Data {
    int16_t data[];
};
layout (binding = 5) buffer Output {
    uint8_t texbuf[];
};

uniform int width;
uniform int height;

#define     ga1         0.490392640201615   // 0.980785280403230
#define     ga2         0.461939766255643   // 0.923879532511287
#define     ga3         0.415734806151272   // 0.831469612302545
#define     ga4         0.353553390593274   // 0.707106781186548
#define     ga5         0.277785116509801   // 0.555570233019602
#define     ga6         0.191341716182545   // 0.382683432365090
#define     ga7         0.097545161008064   // 0.195090322016128

const float c[8][8] =
{
    { ga4,  ga4,  ga4,  ga4,  ga4,  ga4,  ga4,  ga4 },
    { ga1,  ga3,  ga5,  ga7, -ga7, -ga5, -ga3, -ga1 },
    { ga2,  ga6, -ga6, -ga2, -ga2, -ga6,  ga6,  ga2 },
    { ga3, -ga7, -ga1, -ga5,  ga5,  ga1,  ga7, -ga3 },
    { ga4, -ga4, -ga4,  ga4,  ga4, -ga4, -ga4,  ga4 },
    { ga5, -ga1,  ga7,  ga3, -ga3, -ga7,  ga1, -ga5 },
    { ga6, -ga2,  ga2, -ga6, -ga6,  ga2, -ga2,  ga6 },
    { ga7, -ga5,  ga3, -ga1,  ga1, -ga3,  ga5, -ga7 }
};

const int q[8][8] =
{
    {  1,  5,  5,  8, 12, 20, 25, 30 },
    {  6,  6,  7,  9, 13, 29, 30, 27 },
    {  7,  6,  8, 12, 20, 28, 34, 28 },
    {  7,  8, 11, 14, 25, 43, 40, 31 },
    {  9, 11, 18, 28, 34, 54, 51, 38 },
    { 12, 17, 27, 32, 40, 52, 56, 46 },
    { 24, 32, 39, 43, 51, 60, 60, 50 },
    { 36, 46, 47, 45, 56, 50, 51, 49 }
};


shared float transformed[8][8];


void main ()
{
    uint gx = gl_GlobalInvocationID.x;
    uint gy = gl_GlobalInvocationID.y;

    uint lx = gl_LocalInvocationID.x;
    uint ly = gl_LocalInvocationID.y;

    // dequantize
    data[gy * width + gx] *= int16_t (q[ly][lx]);
    barrier ();

    // multiply Ct
    float v = 0.f;
    for (uint i = 0, j = gx + (gy & ~7) * width; i < 8; i ++, j += width)
        v += c[i][ly] * data[j];
    transformed[ly][lx] = v;
    barrier ();

    // multiply C
    v = 0.f;
    for (uint i = 0; i < 8; i ++)
        v += transformed[ly][i] * c[i][lx];

    uint8_t value = v <   0.f ? uint8_t (0)   :
                    v > 255.f ? uint8_t (255) :
                    uint8_t (roundEven (v));

    // Y
    if (gy < height)
        texbuf[gy * width * 2 + gx * 2 + 1] = uint8_t (100); // value;
    else
        // U
        if (gx < (width >> 1))
            texbuf[(gy - height) * width * 2 + gx * 4] = uint8_t (200); // value;
        // V
        else
            texbuf[(gy - height) * 2 * width + (gx - (width >> 1)) * 4 + 2] = value;
}
