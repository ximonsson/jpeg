#include "jpeg/jpeg.h"
#include "ui.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <time.h>
#include <unistd.h>

#define TEST_CMD        "test"
#define COMPRESS_CMD    "compress"
#define DECOMPRESS_CMD  "decompress"
#define DISPLAY_CMD     "display"
#define BILLION         1000000000.f

static int ITERATIONS = 1;

int compress_decompress (const char* cmd, int width, int height, const char* fin, const char* fout)
{
    unsigned char* img;
    unsigned char* dest;
    size_t s;

    // open file and read content
    load_file (fin, (void**) &img, &s);

    // compress
    if (strcmp (cmd, COMPRESS_CMD) == 0)
    {
        struct timespec uno, dos;
        unsigned long int total_duration    = 0;
        unsigned long int shortest_duration = 0xffffffffff;
        unsigned long int longest_duration  = 0;
        unsigned long int duration          = 0;

        dest = malloc (width * height * 2);
        int ret = 0;

        for (int i = 0; i < ITERATIONS; i ++)
        {
            clock_gettime (CLOCK_MONOTONIC, &uno);
            ret = jpeg_compress (img, dest);
            clock_gettime (CLOCK_MONOTONIC, &dos);

            duration = (dos.tv_nsec - uno.tv_nsec) + (dos.tv_sec - uno.tv_sec)  * BILLION;
            total_duration += duration;
            if (duration > longest_duration)
                longest_duration = duration;
            if (duration < shortest_duration)
                shortest_duration = duration;

            draw_ui ();
        }

        printf ("\n");
        printf ("results:\n");
        printf ("done compressing %d images.\n", ITERATIONS);
        printf ("  total time:    %6.4f s\n", (double) total_duration / BILLION);
        printf ("  average time:  %6.4f s\n", (double) total_duration / BILLION / ITERATIONS);
        printf ("  longest time:  %6.4f s\n", (double) longest_duration / BILLION);
        printf ("  shortest time: %6.4f s\n", (double) shortest_duration / BILLION);
        printf ("  fps:           %6.4f  \n", (double) 1.0 / (total_duration / BILLION / ITERATIONS));
        printf ("\n");

        // write to file
        FILE* f = fopen (fout, "wb");
        if (f == NULL)
        {
            fprintf (stderr, "could not open destination file for writing\n");
            return 1;
        }
        fwrite (dest, 1, ret, f);
        fclose (f);
        free (dest);
    }
    // decompress
    else if (strcmp (cmd, DECOMPRESS_CMD) == 0)
    {
        GLuint text;
        get_gl_texture (&text);
        ITERATIONS = atoi (fout);

        struct timespec uno, dos;
        unsigned long int total_duration    = 0;
        unsigned long int shortest_duration = 0xffffffffff;
        unsigned long int longest_duration  = 0;
        unsigned long int duration          = 0;

        dest = malloc (width * height * 2);

        for (int i = 0; i < ITERATIONS; i ++)
        {
            clock_gettime (CLOCK_MONOTONIC, &uno);
            jpeg_decompress_to_texture (img, s, text);
            clock_gettime (CLOCK_MONOTONIC, &dos);

            duration = (dos.tv_nsec - uno.tv_nsec) + (dos.tv_sec - uno.tv_sec)  * BILLION;
            total_duration += duration;
            if (duration > longest_duration)
                longest_duration = duration;
            if (duration < shortest_duration)
                shortest_duration = duration;

            draw_ui ();
        }

        free (dest);

        printf ("\n");
        printf ("results:\n");
        printf ("done decompressing %d images.\n", ITERATIONS);
        printf ("  total time:    %6.4f s\n", (double) total_duration / BILLION);
        printf ("  average time:  %6.4f s\n", (double) total_duration / BILLION / ITERATIONS);
        printf ("  longest time:  %6.4f s\n", (double) longest_duration / BILLION);
        printf ("  shortest time: %6.4f s\n", (double) shortest_duration / BILLION);
        printf ("  fps:           %6.4f  \n", (double) 1.0 / (total_duration / BILLION / ITERATIONS));
        printf ("\n");
    }
    return 0;
}


void print_help (const char* name)
{
    fprintf (stderr, "usage:\n");
    fprintf (stderr, "%s <command> <width> <height> <file> <outfile>\n", name);
    fprintf (stderr, "  commands:\n");
    fprintf (stderr, "     compress       compress <file> to <file>.jpg\n");
    fprintf (stderr, "     decompress     decompress <file>.jpg to <file>.uyvy\n");
    fprintf (stderr, "     display        <width> <height> <file>\n");
    fprintf (stderr, "     test\n");
}


int main (int argc, char** argv)
{
    if (argc < 2)
    {
        print_help (argv[0]);
        exit (1);
    }

    char* cmd = argv[1];
    if (strcmp (cmd, TEST_CMD) == 0)
        exit (0);

    else if (strcmp (cmd, COMPRESS_CMD) == 0 || strcmp (cmd, DECOMPRESS_CMD) == 0)
    {
        if (argc < 6)
        {
            print_help (argv[0]);
            exit (1);
        }

        int width  = atoi (argv[2]);
        int height = atoi (argv[3]);

        if (init_ui (640, 360) != 0)
            return 1;

        GLuint text;
        get_gl_texture (&text);
        // init JPEG
        if (jpeg_init (width, height, text) != 0)
            return 1;

        int ret = compress_decompress (cmd, width, height, argv[4], argv[5]);

#ifdef INTERACTIVE
        SDL_Event event;
        int stop = 0;
        while (!stop)
        {
            while (SDL_PollEvent (&event))
            {
                switch (event.type)
                {
                    case SDL_QUIT:
                        stop = 1;
                    break;
                    case SDL_KEYDOWN:
                        switch (event.key.keysym.sym)
                        {
                            case SDLK_q:
                                stop = 1;
                            break;
                        }
                    break;
                }
            }
            usleep (100);
        }
#endif

        deinit_ui ();
        jpeg_deinit ();
        return ret;
    }
    else fprintf (stderr, "not implemented\n");

    return 0;
}
