#include "ui.h"
#include <stdint.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <GL/gl.h>

#ifdef __OPENGL_ES__
    #define SHADER_DIR  "src/shaders/es"
#else
    #define SHADER_DIR  "src/shaders"
#endif
#define FRAGMENT_SHADER_FILE    SHADER_DIR"/fragment.glsl"
#define VERTEX_SHADER_FILE      SHADER_DIR"/vertex.glsl"
#define MAX_IMAGE_SIZE          1920 * 1080

#define FONT                    "/usr/share/fonts/truetype/ubuntu-font-family/UbuntuMono-B.tff"
#define FONT_SIZE               32

static GLuint           texture,
                        texture_buffer,
                        ttf_texture,
                        vertex_vbo,
                        texture_vbo,
                        color_uniform,
                        program,
                        fragmentshader,
                        vertexshader;

static SDL_Window*      window;
static SDL_GLContext    context;
static TTF_Font*        font;

static int              width,
                        height;


/* default texture coordinates to cover entire shape */
static GLfloat texture_coords[6 * 2] =
{
    0.0, 1.0,
    0.0, 0.0,
    1.0, 0.0,
    1.0, 0.0,
    1.0, 1.0,
    0.0, 1.0
};

/* default vertex coordinates for entire window */
static GLfloat vertex_coords[6 * 3] =
{
    -1.0, -1.0,  0.0,
    -1.0,  1.0,  0.0,
     1.0,  1.0,  0.0,
     1.0,  1.0,  0.0,
     1.0, -1.0,  0.0,
    -1.0, -1.0,  0.0
};

/**
 *  Initialize SDL components.
 */
static int init_sdl ()
{
    if (SDL_Init (SDL_INIT_VIDEO) < 0)
    {
        fprintf (stderr, "error init sdl video\n");
        return 1;
    }

    SDL_GL_SetAttribute (SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute (SDL_GL_DEPTH_SIZE, 24);

    window = SDL_CreateWindow (
        "JPEG decoder",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        width,
        height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
    );

    context = SDL_GL_CreateContext (window);
    SDL_GL_SetSwapInterval (1);

    if (TTF_Init() != 0)
    {
        const char* ttf_error = TTF_GetError();
        fprintf (stderr, "ttf error: %s", ttf_error);
        return 1;
    }
    font = TTF_OpenFont (FONT, FONT_SIZE);
    return 0;
}

/**
 *  Print log to stderr over compilation of the shader.
 */
static void print_shader_compile_log (GLuint shader)
{
    GLint log_size = 0;
    glGetShaderiv (shader, GL_INFO_LOG_LENGTH, &log_size);

    char error_log[log_size];
    glGetShaderInfoLog (shader, log_size, &log_size, error_log);

    fprintf (stderr, "error compiling shader\n%s\n", error_log);
}

/**
 *  Compile a shader from source file.
 *  The shader must have previously been created with glCreateShader.
 *  Returns non-zero on error.
 */
int compile_shader (GLuint shader, const char* source_file)
{
    int status, result;
    size_t file_size;
    char* source;

    // read shader source
    FILE * fp = fopen (source_file, "rb");
    if (!fp)
    {
        fprintf (stderr, "could not open shader file\n");
        return 1;
    }
    fseek (fp, 0, SEEK_END);
    file_size = ftell (fp) + 1;
    rewind (fp);
    source = (char *) malloc (file_size);
    memset (source, 0, file_size);

    if ((result = fread (source, 1, file_size - 1, fp)) != file_size - 1)
    {
        fprintf (stderr, "could not read entire vertex shader\n");
        return 1;
    }
    fclose (fp);

    // set source and compile shader
    glShaderSource  (shader, 1, (const char**) &source, 0);
    glCompileShader (shader);
    glGetShaderiv   (shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE)
    {
        print_shader_compile_log (shader);
        free (source);
        return 1;
    }

    free (source);
    return 0;
}

/**
 *  Build main shader program.
 *  Compiles shaders and links them together.
 */
static int build_program ()
{
    vertexshader = glCreateShader (GL_VERTEX_SHADER);
    if (compile_shader (vertexshader, VERTEX_SHADER_FILE) != 0)
        return 1;

    fragmentshader = glCreateShader (GL_FRAGMENT_SHADER);
    if (compile_shader (fragmentshader, FRAGMENT_SHADER_FILE) != 0)
        return 1;

    // create the shader program and attach the shaders
    program = glCreateProgram ();
    glAttachShader (program, fragmentshader);
    glAttachShader (program, vertexshader);
    glLinkProgram  (program);
    glUseProgram   (program);

    return 0;
}

/**
 *  Initialize OpenGL components.
 *  Builds shader program, creates textures, vertex buffers and sets up coordinate space.
 */
static int init_opengl ()
{
    if (build_program () != 0)
        return 1;

    glClearColor (0.f, .6f, .5f, 1.f);
    glViewport   (0, 0, width, height);

    // generate texture buffer
    glEnable        (GL_TEXTURE_2D);
    glActiveTexture (GL_TEXTURE0);
    glGenTextures   (1, &texture);
#ifdef __OPENGL_ES__ // use ordinary texture OpenGL ES
    glBindTexture   (GL_TEXTURE_2D, texture);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
#else // use texture buffer for standard OpenGL
    glGenBuffers    (1, &texture_buffer);
    glBindBuffer    (GL_TEXTURE_BUFFER, texture_buffer);
    glBufferData    (GL_TEXTURE_BUFFER, MAX_IMAGE_SIZE * 2, NULL, GL_DYNAMIC_DRAW);
    glBindTexture   (GL_TEXTURE_BUFFER, texture);
    glTexBuffer     (GL_TEXTURE_BUFFER, GL_RGBA8, texture_buffer);
    glTexParameteri (GL_TEXTURE_BUFFER, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_BUFFER, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_BUFFER, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
    glTexParameteri (GL_TEXTURE_BUFFER, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
#endif

    glActiveTexture (GL_TEXTURE1);
    glGenTextures   (1, &ttf_texture);

    // generate vertex vbo
    glGenBuffers (1, &vertex_vbo);
    glBindBuffer (GL_ARRAY_BUFFER, vertex_vbo);
    glBufferData (GL_ARRAY_BUFFER, 6 * 3 * sizeof (GLfloat), vertex_coords, GL_STATIC_DRAW);

    GLuint vertex_attrib_location = glGetAttribLocation (program, "vertex");
    glEnableVertexAttribArray (vertex_attrib_location);
    glVertexAttribPointer (vertex_attrib_location, 3, GL_FLOAT, 0, 0, 0);

    // generate texture vbo
    glGenBuffers (1, &texture_vbo);
    glBindBuffer (GL_ARRAY_BUFFER, texture_vbo);
    glBufferData (GL_ARRAY_BUFFER, 6 * 2 * sizeof (GLfloat), texture_coords, GL_STATIC_DRAW);

    GLuint texture_attrib_location = glGetAttribLocation (program, "texture_coords_in");
    glEnableVertexAttribArray (texture_attrib_location);
    glVertexAttribPointer (texture_attrib_location, 2, GL_FLOAT, 0, 0, 0);

    // set uniform data
    glUniform1i (glGetUniformLocation (program, "texbuf"), 0);
    glUniform1i (glGetUniformLocation (program, "tex"),    1);

    // set color to white
    color_uniform = glGetUniformLocation (program, "color_in");
    glUniform4f (color_uniform, 1.0, 1.0, 1.0, 1.0);
    return 0;
}

/**
 *  Destroy SDL components.
 */
static void deinit_sdl ()
{
    TTF_CloseFont (font);
    SDL_GL_DeleteContext (context);
    SDL_DestroyWindow (window);
    SDL_Quit ();
    TTF_Quit ();
}

/**
 *  Destroy OpenGL components
 */
static void deinit_opengl ()
{
    glDeleteTextures (1, &texture);
    glDeleteTextures (1, &ttf_texture);
    glDeleteShader   (vertexshader);
    glDeleteShader   (fragmentshader);
    glDeleteProgram  (program);

    GLuint buffers[2] = { texture_vbo, vertex_vbo };
    glDeleteBuffers (2, buffers);
}

// static int count = 0;
void draw_ui ()
{
    glClear                 (GL_COLOR_BUFFER_BIT);
    glUseProgram            (program);
#ifndef __OPENGL_ES__
    GLuint index = glGetSubroutineIndex (program, GL_FRAGMENT_SHADER, "render_uyvy_texbuf");
    glUniformSubroutinesuiv (GL_FRAGMENT_SHADER, 1, &index );
    glBindTexture (GL_TEXTURE_BUFFER, texture);
#else
    glBindTexture (GL_TEXTURE_2D, texture);
#endif
    glBindBuffer  (GL_ARRAY_BUFFER, vertex_vbo);
    glBufferData  (GL_ARRAY_BUFFER, 6 * 3 * sizeof (GLfloat), vertex_coords, GL_STATIC_DRAW);
    glDrawArrays  (GL_TRIANGLES, 0, 6);
    SDL_GL_SwapWindow (window);
}


void load_texture (uint8_t* data, int width, int height)
{
    glUniform1i   (glGetUniformLocation (program, "width"),  width >> 1);
    glUniform1i   (glGetUniformLocation (program, "height"), height);
#ifdef __OPENGL_ES__
    glBindTexture (GL_TEXTURE_2D, texture);
    glTexImage2D  (GL_TEXTURE_2D, 0, GL_RGBA8, width >> 1, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
#else
    glBindBuffer  (GL_TEXTURE_BUFFER, texture_buffer);
    if (data)
        glBufferData (GL_TEXTURE_BUFFER, width * height * 2, data, GL_STATIC_DRAW);
#endif
}


int init_ui (int w, int h)
{
    int ret;
    width  = w;
    height = h;

    ret = init_sdl ();
    if (ret != 0)
        return 1;

    ret = init_opengl ();
    if (ret != 0)
        return 1;

    return 0;
}


void deinit_ui ()
{
    deinit_opengl ();
    deinit_sdl    ();
}


void get_gl_texture (GLuint* tex)
{
    *tex = texture_buffer;
}
