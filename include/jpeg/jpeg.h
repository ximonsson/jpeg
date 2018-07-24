/** ----------------------------------------------------------------
 *  File:
 *      jpeg.h
 *  Description:
 *      Include file for the JPEG codec.
 *      The functions are implemented differently for either
 *      standard/assembly/hardware.
 * ----------------------------------------------------------------- */
#ifndef _JPEG_H
#define _JPEG_H

#include <stdio.h>
#include <GL/gl.h>

#define JPEG_HEADER_SIZE    16
#define JPEG_BLOCK_SIZE      8

/**
 *  Init JPEG compression and decompression.
 *  Returns non-zero value on error.
 */
int jpeg_init (int w, int h, GLuint /* texture */) ;

/**
 * Deinitialize the JPEG library and free any memory allocated by it.
 */
void jpeg_deinit () ;

/**
 *  Encode data to destination.
 *  Returns size of compressed data. A zero value indicates
 *  an error occurred.
 */
int jpeg_compress (unsigned char* data, unsigned char* destination) ;

/**
 *  Compress data in the current texture to destination buffer.
 */
int jpeg_compress_from_texture (GLuint texture, unsigned char* destination) ;

/**
 *  Decode data into destination.
 *  Returns non-zero value on error.
 */
int jpeg_decompress (unsigned char* data, size_t size, unsigned char* destination) ;

/**
 *  Decompress data and load it to OpenGL texture, ready for rendering.
 */
int jpeg_decompress_to_texture (unsigned char* data, size_t size, GLuint texture) ;

#endif
