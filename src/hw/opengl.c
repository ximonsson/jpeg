#include "ui.h"
#include "gpu.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#define COMPUTE_SHADER_SOURCE_FILE  "src/shaders/jpeg.glsl"


static GLuint blocks_buffer, output_buffer;
static int width, height;
static GLuint shader, decompress_program;


int init_compute_shader (int w, int h)
{
    printf ("compiling compute shader... ");
    width = w; height = h;

    // create and compile compute shader
    shader = glCreateShader (GL_COMPUTE_SHADER);
    if (compile_shader (shader, COMPUTE_SHADER_SOURCE_FILE) != 0)
        return 1;

    // creating decompress_program
    decompress_program = glCreateProgram ();
    glAttachShader (decompress_program, shader);
    glLinkProgram  (decompress_program);
    glUseProgram   (decompress_program);

    glUniform1i (glGetUniformLocation (decompress_program, "width"), width);
    glUniform1i (glGetUniformLocation (decompress_program, "height"), height);
    // glUniform1ui (glGetUniformLocation (decompress_program, "tex"), 0);

    printf ("done\n");

    // create blocks_buffer
    glGenBuffers     (1, &blocks_buffer);
    glBindBufferBase (GL_SHADER_STORAGE_BUFFER, 4, blocks_buffer);

    // create output buffer
    glGenBuffers     (1, &output_buffer);
    glBindBuffer     (GL_SHADER_STORAGE_BUFFER, output_buffer);
    glBufferData     (GL_SHADER_STORAGE_BUFFER, width * height * 2, NULL, GL_DYNAMIC_COPY);

    return 0;
}

#ifdef JPEG_HW__USE_GLSL
void decompress_blocks_to_texture (int16_t* blocks, GLuint texture_buffer)
{
    glUseProgram       (decompress_program);
    glBindBuffer       (GL_SHADER_STORAGE_BUFFER, blocks_buffer);
    glBufferData       (GL_SHADER_STORAGE_BUFFER, width * height * 2 * sizeof (int16_t), blocks, GL_STREAM_COPY);

    glBindBuffer       (GL_TEXTURE_BUFFER, texture_buffer);
    glBindBufferBase   (GL_TEXTURE_BUFFER, 5, texture_buffer);
    glDispatchCompute  (width >> 3, height >> 2, 1);
}

void decompress_blocks (int16_t* blocks, uint8_t* destination)
{
    glUseProgram       (decompress_program);
    glBindBuffer       (GL_SHADER_STORAGE_BUFFER, blocks_buffer);
    glBufferData       (GL_SHADER_STORAGE_BUFFER, width * height * 2 * sizeof (int16_t), blocks, GL_STREAM_COPY);

    glBindBuffer       (GL_SHADER_STORAGE_BUFFER, output_buffer);
    glBindBufferBase   (GL_SHADER_STORAGE_BUFFER, 5, output_buffer);
    glDispatchCompute  (width >> 3, height >> 2, 1);

    int8_t* output = glMapBuffer (GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
    memcpy (destination, output, width * height * 2);
}
#endif

void deinit_compute_shader ()
{
    glDeleteShader   (shader);
    glDeleteProgram  (decompress_program);
}
