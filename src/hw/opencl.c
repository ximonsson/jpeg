#include "utils.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>
#include <CL/cl.h>
#include <CL/cl_gl.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include "gpu.h"


#define KERNEL_SRC              "src/kernels/jpeg.cl"
#define DECOMPRESS_KERNEL       "decompress"
#define COMPRESS_KERNEL         "compress"
#define WORK_DIMENSIONS         2
#define BUFFER_SIZE             width * height * 2
#define PLATFORM                0
#define BLOCK_SIZE              8


static cl_context       context;

static cl_mem           device_inbuffer,
                        device_outbuffer,
                        texture_buffer;

static cl_program       program;

static cl_kernel        decompress_kernel,
                        compress_kernel;

static cl_command_queue queue;

static int              width,
                        height;


/**
 *  Check error code and return if it was success or not.
 *  Prints error message in case of error.
 *  Returns non-zero if error code is an error.
 */
static inline int check_error (cl_int error, const char* name)
{
    if (error != CL_SUCCESS)
    {
        printf ("error!\n");
        fprintf (stderr, "  CL error: %s (%d)\n", name, error);
        return 1;
    }
    return 0;
}

/**
 *  Load a source file at fpath.
 *  source_buffer will point to an allocated buffer that needs to be freed later.
 *  size will contain the size of the source_buffer.
 */
static int load_source_file (const char* fpath, char** source_buffer, size_t* size)
{
    int ret = 0;
    // open file
    if ((ret = load_file (fpath, (void**) source_buffer, size)) != 0)
        perror ("error loading kernel\n");
    return ret;
}

/**
 *  Create kernel from name
 */
static int create_kernel (const char* name, cl_kernel* kernel)
{
    cl_int ret;
    *kernel = clCreateKernel (program, name, &ret);
    if (check_error (ret, "clCreateKernel") != 0)
        return 1;
    return 0;
}

/**
 *  Create our decompress kernel.
 */
static int create_decompress_kernel ()
{
    if (create_kernel (DECOMPRESS_KERNEL, &decompress_kernel) != 0)
        return 1;

    // set kernel arguments
    clSetKernelArg (decompress_kernel, 0, sizeof (cl_mem), &device_inbuffer);
    // NOTE: we set output buffer later...
    clSetKernelArg (decompress_kernel, 2, sizeof (int),    &width);
    clSetKernelArg (decompress_kernel, 3, sizeof (int),    &height);

    return 0;
}

/**
 *  Create compression kernel.
 */
static int create_compress_kernel ()
{
    if (create_kernel (COMPRESS_KERNEL, &compress_kernel) != 0)
        return 1;

    // set kernel arguments
    clSetKernelArg (compress_kernel, 0, sizeof (cl_mem), &device_inbuffer);
    clSetKernelArg (compress_kernel, 1, sizeof (int),    &width);
    clSetKernelArg (compress_kernel, 2, sizeof (int),    &height);
    return 0;
}

/**
 *  Initialize OpenCL components.
 */
int init_opencl (int width_, int height_, GLuint texture)
{
    cl_int ret;
    cl_uint count;
    cl_platform_id platform_ids[2];

    width = width_; height = height_;

    // Get platforms
    ret = clGetPlatformIDs (2, platform_ids, &count);
    if (check_error (ret, "clGetPlatformIDs") != 0)
        return 1;

    // Create context on devices
    const cl_context_properties ctx_properties[] =
    {
        CL_GL_CONTEXT_KHR,   (cl_context_properties) glXGetCurrentContext(),
        CL_GLX_DISPLAY_KHR,  (cl_context_properties) glXGetCurrentDisplay(),
        CL_CONTEXT_PLATFORM, (cl_context_properties) platform_ids[PLATFORM],
        0
    };
    cl_device_id device_ids[1];
    // size_t ndevices;
    // clGetGLContextInfoKHR (ctx_properties, CL_DEVICES_FOR_GL_CONTEXT_KHR, 1 * sizeof (cl_device_id), device_ids, &ndevices);
    ret = clGetDeviceIDs (platform_ids[PLATFORM], CL_DEVICE_TYPE_ALL, 1, device_ids, &count);
    if (check_error (ret, "clGetDeviceIDs") != 0)
        return 1;

    context = clCreateContext (ctx_properties, 1, device_ids, NULL, NULL, &ret);
    if (check_error (ret, "clCreateContext") != 0)
        return 1;

    queue = clCreateCommandQueue (context, device_ids[0], 0, &ret);
    if (check_error (ret, "clCreateCommandQueue") != 0)
        return 1;

    // Create buffers to device
    //      input buffer
    device_inbuffer = clCreateBuffer (
        context,
        CL_MEM_READ_ONLY,
        BUFFER_SIZE * sizeof (short),
        NULL,
        &ret
    );
    if (check_error (ret, "clCreateBuffer") != 0)
        return 1;

    //      output buffer
    device_outbuffer = clCreateBuffer (
        context,
        CL_MEM_WRITE_ONLY,
        BUFFER_SIZE * sizeof (short),
        NULL,
        &ret
    );
    if (check_error (ret, "clCreateBuffer") != 0)
        return 1;

    // create a buffer towards the GL buffer
    texture_buffer = clCreateFromGLBuffer (
        context,
        CL_MEM_WRITE_ONLY,
        texture,
        &ret
    );
    if (check_error (ret, "clCreateFromGLBuffer") != 0)
        return 1;

    // Create a program from source code
    //      load source file
    char* src;
    size_t program_size;
    if (load_source_file (KERNEL_SRC, &src, &program_size) != 0)
        return 1;

    //      create the program
    program = clCreateProgramWithSource (context, 1, (const char**) &src, &program_size, &ret);
    if (check_error (ret, "clCreateProgramWithSource") != 0)
        return 1;

    //      build and compile the program
    ret = clBuildProgram (program, count, device_ids, NULL, NULL, NULL);
    if (check_error (ret, "clBuildProgram") != 0)
    {
        // get log information
        size_t log_size;
        clGetProgramBuildInfo (program, device_ids[0], CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
        // Allocate memory for the log
        char* log = (char*) malloc (log_size);
        memset (log, 0, log_size);
        // Get the log
        clGetProgramBuildInfo (program, device_ids[0], CL_PROGRAM_BUILD_LOG, log_size, log, NULL);
        // Print the log
        fprintf (stderr, "%s\n", log);
        free (log);
        return 1;
    }

    // create kernels
    if (create_decompress_kernel () != 0)
        return 1;

    if (create_compress_kernel () != 0)
        return 1;

    // cleanup
    free (src);
    return 0;
}

void deinit_opencl ()
{
    clReleaseMemObject      (texture_buffer);
    clReleaseMemObject      (device_inbuffer);
    clReleaseMemObject      (device_outbuffer);
    clReleaseCommandQueue   (queue);
    clReleaseKernel         (decompress_kernel);
    clReleaseKernel         (compress_kernel);
    clReleaseProgram        (program);
    clReleaseContext        (context);
}

#ifdef JPEG_HW__USE_OPENCL
void decompress_blocks (int16_t* blocks, uint8_t* destination)
{
    cl_uint ret;
    // set dimensions of the work
    size_t global_work_size[] = { width, height << 1, 0 };
    size_t local_work_size [] = { BLOCK_SIZE, BLOCK_SIZE, 0 };

    // device set output of kernel to hosted buffer
    clSetKernelArg (decompress_kernel, 1, sizeof (cl_mem), &device_outbuffer);

    // write data to GPU
    ret = clEnqueueWriteBuffer (
        queue, device_inbuffer, CL_TRUE, 0, BUFFER_SIZE * sizeof (int16_t),
        blocks, 0, NULL, NULL
    );
    if (check_error (ret, "clEnqueueWriteBuffer") != 0)
        return;

    // decompress
    ret = clEnqueueNDRangeKernel (
        queue, decompress_kernel, WORK_DIMENSIONS, NULL, global_work_size,
        local_work_size, 0, NULL, NULL
    );
    if (check_error (ret, "clEnqueueNDRangeKernel") != 0)
        return;

    ret = clEnqueueReadBuffer (
        queue, device_outbuffer, CL_TRUE, 0, BUFFER_SIZE, destination,
        0, NULL, NULL
    );
    if (check_error (ret, "clEnqueueReadBuffer") != 0)
        return;
}

void decompress_blocks_to_texture (int16_t* blocks, GLuint texture)
{
    cl_int ret;
    // set dimensions of the work
    size_t global_work_size[] = { width, height << 1, 0 };
    size_t local_work_size [] = { BLOCK_SIZE, BLOCK_SIZE, 0 };

    // set output to OpenGL buffer
    clSetKernelArg (decompress_kernel, 1, sizeof (cl_mem), &texture_buffer);

    // write data to GPU
    ret = clEnqueueWriteBuffer (
        queue, device_inbuffer, CL_TRUE, 0, BUFFER_SIZE * sizeof (int16_t),
        blocks, 0, NULL, NULL
    );
    if (check_error (ret, "clEnqueueWriteBuffer") != 0)
        return;

    // take control of GL objects
    ret = clEnqueueAcquireGLObjects (
        queue, 1, &texture_buffer, 0, NULL, NULL
    );
    if (check_error (ret, "clEnqueueAcquireGLObjects") != 0)
        return;

    // decompress
    ret = clEnqueueNDRangeKernel (
        queue, decompress_kernel, WORK_DIMENSIONS, NULL, global_work_size,
        local_work_size, 0, NULL, NULL
    );
    if (check_error (ret, "clEnqueueNDRangeKernel") != 0)
        return;

    // release OpenGL objects
    ret = clEnqueueReleaseGLObjects (
        queue, 1, &texture_buffer, 0, NULL, NULL
    );
    check_error (ret, "clEnqueueReleaseGLObjects");

    load_texture (NULL, width, height);
}
#endif // JPEG_HW__USE_OPENCL

void compress_blocks (int16_t* blocks)
{
    cl_uint ret;
    // set dimensions of the work
    size_t global_work_size[] = { width, height << 1, 0 };
    size_t local_work_size [] = { BLOCK_SIZE, BLOCK_SIZE, 0 };

    // write data to GPU
    ret = clEnqueueWriteBuffer (
        queue, device_inbuffer, CL_TRUE, 0, BUFFER_SIZE * sizeof (int16_t),
        blocks, 0, NULL, NULL
    );
    if (check_error (ret, "clEnqueueWriteBuffer") != 0)
        return;

    // decompress
    ret = clEnqueueNDRangeKernel (
        queue, compress_kernel, WORK_DIMENSIONS, NULL, global_work_size,
        local_work_size, 0, NULL, NULL
    );
    if (check_error (ret, "clEnqueueNDRangeKernel") != 0)
        return;

    ret = clEnqueueReadBuffer (
        queue, device_inbuffer, CL_TRUE, 0, BUFFER_SIZE * sizeof (int16_t),
        blocks, 0, NULL, NULL
    );
    if (check_error (ret, "clEnqueueReadBuffer") != 0)
        return;
}
