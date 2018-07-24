#include "ui.h"
#include "jpeg/jpeg.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>


#define MULTIFILES_PATH     "video/multi"
#define SINGLEFILE_PATH     "video/single"
#define BILLION             1000000000.f
#define ITERATIONS          1500


static FILE* fp;
static FILE* ifp;


static int get_frame__multi (int frame, uint8_t* data, size_t* size)
{
    char path[256] = {0};
    sprintf (path, "%s/%d.jpg", MULTIFILES_PATH, frame + 1);
    fp = fopen (path, "rb");
    fseek (fp, 0, SEEK_END);
    *size = ftell (fp);
    fseek (fp, 0, SEEK_SET);
    fread (data, *size, 1, fp);
    fclose (fp);
    return 0;
}


static int get_frame__single (int frame, uint8_t* data, size_t* size)
{
    int pos, s;
    fseek (ifp, frame * sizeof (int) * 2, SEEK_SET);
    fread (&pos, 1, sizeof (int), ifp);
    fread (&s, 1, sizeof (int), ifp);

    *size = s;

    fseek (fp, pos, SEEK_SET);
    fread (data, *size, 1, fp);

    return 0;
}


static int play_multi ()
{
    uint8_t* jpeg_data = malloc (1920 * 1080 * 2);
    size_t size;

    struct timespec uno, dos;
    unsigned long int total_duration    = 0;
    unsigned long int shortest_duration = 0xffffffffff;
    unsigned long int longest_duration  = 0;
    unsigned long int duration          = 0;

    for (int i = 0; i < ITERATIONS; i ++)
    {
        clock_gettime (CLOCK_MONOTONIC, &uno);
        get_frame__multi (i, jpeg_data, &size);
        jpeg_decompress_to_texture (jpeg_data, size, 0);
        clock_gettime (CLOCK_MONOTONIC, &dos);
        draw_ui ();

        duration = (dos.tv_nsec - uno.tv_nsec) + (dos.tv_sec - uno.tv_sec)  * BILLION;
        total_duration += duration;
        if (duration > longest_duration)
            longest_duration = duration;
        if (duration < shortest_duration)
            shortest_duration = duration;
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

    free (jpeg_data);

    return 0;
}


static int play_single ()
{
    fp  = fopen (SINGLEFILE_PATH"/video.mjpg", "rb");
    ifp = fopen (SINGLEFILE_PATH"/index", "rb");
    uint8_t* jpeg_data = malloc (1920 * 1080 * 2);
    size_t size;

    struct timespec uno, dos;
    unsigned long int total_duration    = 0;
    unsigned long int shortest_duration = 0xffffffffff;
    unsigned long int longest_duration  = 0;
    unsigned long int duration          = 0;

    for (int i = 0; i < ITERATIONS; i ++)
    {
        clock_gettime (CLOCK_MONOTONIC, &uno);
        get_frame__single (i, jpeg_data, &size);
        jpeg_decompress_to_texture (jpeg_data, size, 0);
        clock_gettime (CLOCK_MONOTONIC, &dos);
        draw_ui ();

        duration = (dos.tv_nsec - uno.tv_nsec) + (dos.tv_sec - uno.tv_sec)  * BILLION;
        total_duration += duration;
        if (duration > longest_duration)
            longest_duration = duration;
        if (duration < shortest_duration)
            shortest_duration = duration;
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

    fclose (fp);
    fclose (ifp);
    free (jpeg_data);
    return 0;
}



int main (int argc, char** argv)
{
    int ret;
    init_ui (960, 540);
    jpeg_init (1920, 1080, 0);

    if (strcmp (argv[1], "multi") == 0)
    {
        printf ("playing multi file version...\n");
        ret = play_multi ();
    }
    else if (strcmp (argv[1], "single") == 0)
    {
        printf ("playing single file version...\n");
        ret = play_single ();
    }
    else
    {
        ret = 1;
    }

    deinit_ui ();
    jpeg_deinit ();
    return ret;
}
