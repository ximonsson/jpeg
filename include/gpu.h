/**
 *  File: opencl.h
 *  Description:
 *      API for JPEG decompression through OpenCL.
 */

#ifndef __JPEG_GPU_H
#define __JPEG_GPU_H

#include <stdint.h>

/**
 *  Initialize OpenCL components for GPGPU.
 *  Returns non-zero on failure on connecting to the device.
 */
int init_opencl (int /* width */, int /* height */, GLuint /* texture */) ;

/**
 *  Deinitialize OpenCL components.
 */
void deinit_opencl ();

/**
 *  Decompress blocks from pointer blocks into destination buffer.
 */
void decompress_blocks (int16_t* /* blocks */, uint8_t* /* destination */) ;

/**
 *  Decompress from memory into OpenGL texture
 */
void decompress_blocks_to_texture (int16_t* /* blocks */, GLuint /* texture */) ;

/**
 *  Compress blocks from source into destination ready for encoding.
 */
void compress_blocks (int16_t* /* blocks */);

/**
 *  Initialize compute shader for JPEG decompression to texture
 */
int init_compute_shader (int w, int h) ;

/**
 *  Deinitialize the compute shader.
 */
void deinit_compute_shader () ;

#endif /* __JPEG_GPU_H */
