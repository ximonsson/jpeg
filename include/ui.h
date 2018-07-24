/**
 *  File: ui.h
 *  Description: User interface components API.
 */
#ifndef _UI_H
#define _UI_H

#include <GL/gl.h>

/**
 *  Initialize UI components.
 *  SDL and OpenGL.
 */
int init_ui (int /* width */, int /* height */) ;

/**
 *  Deinitialize UI components.
 */
void deinit_ui () ;

/**
 *  Load new texture to be drawn.
 */
void load_texture (unsigned char* /* data */, int /* width */, int /* height */) ;

/**
 *  Retrieve current GL texture.
 */
void get_gl_texture (GLuint* /* texture */) ;

/**
 *  Re-draw graphics.
 */
void draw_ui () ;

/**
 *  Compile a shader from source file.
 *  Shader needs to have been previously created with glCreateShader.
 */
int compile_shader (GLuint /* shader */, const char* /* source file */) ;

#endif
