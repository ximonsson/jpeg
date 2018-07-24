/**
 *  File: jpeg.cl
 *  Description:
 *      OpenCL kernel modules used to perform JPEG (de)compression.
 */

#define     ga1         0.490392640201615   // 0.980785280403230
#define     ga2         0.461939766255643   // 0.923879532511287
#define     ga3         0.415734806151272   // 0.831469612302545
#define     ga4         0.353553390593274   // 0.707106781186548
#define     ga5         0.277785116509801   // 0.555570233019602
#define     ga6         0.191341716182545   // 0.382683432365090
#define     ga7         0.097545161008064   // 0.195090322016128

#define     BLOCK_SIZE  8


/* C matrix for making transformations between frequency domain and spatial domain. */
__constant float c[8][8] =
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

/* Quantization matrix */
// Note: [0] = 8.0
__constant short q[8][8] =
{
    {  1.0,  5.0,  5.0,  8.0, 12.0, 20.0, 25.0, 30.0 },
    {  6.0,  6.0,  7.0,  9.0, 13.0, 29.0, 30.0, 27.0 },
    {  7.0,  6.0,  8.0, 12.0, 20.0, 28.0, 34.0, 28.0 },
    {  7.0,  8.0, 11.0, 14.0, 25.0, 43.0, 40.0, 31.0 },
    {  9.0, 11.0, 18.0, 28.0, 34.0, 54.0, 51.0, 38.0 },
    { 12.0, 17.0, 27.0, 32.0, 40.0, 52.0, 56.0, 46.0 },
    { 24.0, 32.0, 39.0, 43.0, 51.0, 60.0, 60.0, 50.0 },
    { 36.0, 46.0, 47.0, 45.0, 56.0, 50.0, 51.0, 49.0 }
};

/**
 *  Decompress JPEG data to destination buffer.
 */
__kernel void decompress (__global short* data,
                          __global uchar* destination,
                             const int    width,
                             const int    height)
{
    __local float transformed[BLOCK_SIZE][BLOCK_SIZE];

    int x  = get_global_id (0);
    int y  = get_global_id (1);
    int lx = get_local_id  (0);
    int ly = get_local_id  (1);

    // dequantize
    data[y * width + x] *= q[ly][lx];
    barrier (CLK_LOCAL_MEM_FENCE);

    // mutlitply Ct
    __global short* block = &data[x + (y & ~7) * width];
    float v = 0.f;
    for (int i = 0, j = 0; i < BLOCK_SIZE; i ++, j += width)
        v += c[i][ly] * block[j];
    transformed[ly][lx] = v;
    barrier (CLK_LOCAL_MEM_FENCE);

    // multiply C
    v = 0.f;
    for (int i = 0; i < BLOCK_SIZE; i ++)
        v += transformed[ly][i] * c[i][lx];
    uchar value = convert_uchar_sat_rte (v);

    // unpack and interleave values to destination buffer
    // Y
    if (y < height)
        destination[y * width * 2 + x * 2 + 1] = value;
    else
        // U
        if (x < width >> 1)
            destination[(y - height) * width * 2 + x * 4] = value;
        // V
        else
            destination[(y - height) * width * 2 + (x - (width >> 1)) * 4 + 2] = value;
}


__kernel void compress (__global short* data,
                           const int    width,
                           const int    height)
{
    __local float transformed[BLOCK_SIZE][BLOCK_SIZE];

    int x  = get_global_id (0);
    int y  = get_global_id (1);
    int lx = get_local_id  (0);
    int ly = get_local_id  (1);

    __global short* col = &data[x + (y & ~7) * width];

    // multiply C
    float v = 0.f;
    for (int i = 0, j = 0; i < BLOCK_SIZE; i ++, j += width)
        v += c[ly][i] * col[j];

    transformed[ly][lx] = v;
    barrier (CLK_LOCAL_MEM_FENCE);

    // mutlitply Ct
    v = 0.f;
    for (int i = 0; i < BLOCK_SIZE; i ++)
        v += transformed[ly][i] * c[lx][i];

    // quantize + normalize
    if (ly == 0 && lx == 0)
        v = (v - 1024.0) / 8.0;
    else
        v /= q[ly][lx];
    data[y * width + x] = convert_short_sat_rte (v);
}
