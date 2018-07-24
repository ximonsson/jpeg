#include "ui.h"
#include <stdio.h>
#include <string.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <SDL2/SDL.h>


#define PIX_FMT             AV_PIX_FMT_YUV422P
#define BILLION             1000000000.f


static AVFormatContext*     fmt_ctx;
static AVStream*            stream;
static AVCodecContext*      decoder_ctx;
static AVCodec*             decoder;
static AVPacket             packet;
static AVFrame*             frame;
static struct SwsContext*   sws_ctx;

static int                  stream_index;
static uint8_t*             video_dst_data[4] = { NULL };
static int                  video_dst_linesize[4];
static int                  destination_buffer_size;
static uint8_t*             planar_image;
static int                  stop;
static int64_t              time_base;

static SDL_Event            event;


/**
 *  Open video source and setup for demuxing and transcoding.
 *  Returns non-zero on error.
 */
static int open (const char* source)
{
    int ret = 0;

    // open source file
    if (avformat_open_input (&fmt_ctx, source, NULL, NULL) < 0)
    {
        fprintf (stderr, "could not open %s\n", source);
        return 1;
    }

    // find stream information
    if (avformat_find_stream_info (fmt_ctx, NULL) < 0)
    {
        fprintf (stderr, "could not find stream information\n");
        return 1;
    }

    // search for video stream
    if ((ret = av_find_best_stream (fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0)) < 0)
    {
        fprintf (stderr, "could not find video stream\n");
        return 1;
    }

    // get decoder
    stream_index = ret;
    stream       = fmt_ctx->streams[stream_index];
    decoder_ctx  = stream->codec;
    decoder      = avcodec_find_decoder (decoder_ctx->codec_id);

    if (!decoder)
    {
        fprintf (stderr, "could not find decoder\n");
        return 1;
    }

    // open codec
    if ((ret = avcodec_open2 (decoder_ctx, decoder, NULL)) < 0)
    {
        fprintf (stderr, "failed to open video codec\n");
        return 1;
    }

    // dump information to stderr
    av_dump_format (fmt_ctx, 0, source, 0);

    // allocate frame
    frame = av_frame_alloc ();
    if (!frame)
    {
        fprintf (stderr, "failed to allocate frame\n");
        return 1;
    }

    // init packet struct
    av_init_packet (&packet);
    packet.size = 0;
    packet.data = NULL;

    if (decoder_ctx->pix_fmt != PIX_FMT)
    {
        // allocate image as decoding destination
        ret = av_image_alloc (
            video_dst_data,
            video_dst_linesize,
            decoder_ctx->width,
            decoder_ctx->height,
            PIX_FMT,
            1
        );
        if (ret < 0)
        {
            fprintf (stderr, "failed to allocate destination image for decoding\n");
            return 1;
        }
        destination_buffer_size = ret;

        // allocate sws context to convert pixel format
        sws_ctx = sws_getContext (
            decoder_ctx->width,
            decoder_ctx->height,
            decoder_ctx->pix_fmt,
            decoder_ctx->width,
            decoder_ctx->height,
            PIX_FMT,
            SWS_BILINEAR,
            NULL,
            NULL,
            NULL
        );
    }

    // calculate time base used for seeking for a frame
    time_base = decoder_ctx->time_base.num * AV_TIME_BASE / decoder_ctx->time_base.den;

    return 0;
}

/**
 *  Close the video source.
 */
static int close ()
{
    avformat_free_context (fmt_ctx);
    av_freep (&video_dst_data[0]);
    return 0;
}

/**
 *  Seek to frame position.
 *  Returns zero on success.
 */
static int seek (int64_t pts)
{
    // int flags = AVSEEK_FLAG_ANY | AVSEEK_FLAG_BACKWARD;
    int flags = AVSEEK_FLAG_BACKWARD;
    // int flags = 0;
    int ret = av_seek_frame (fmt_ctx, stream_index, pts, flags);
    if (ret < 0)
    {
        fprintf (stderr, "error seeking frame (%d)\n", ret);
        return 1;
    }
    return 0;
}

/**
 *  Seek to a random frame.
 */
static int random_seek (int64_t* pts)
{
    // return 0;
    // *pts = rand() % stream->duration;
    *pts = rand() % 10 ;
    // *pts = rand() % 14350;
    if (stream->start_time != (int64_t) AV_NOPTS_VALUE)
        *pts += stream->start_time;
    *pts *= time_base;
    // *pts *= AV_TIME_BASE;
    // *pts = ((*pts) * decoder_ctx->time_base.num) / decoder_ctx->time_base.den;
    return seek (*pts);
}

/**
 *  Compress the current frame to dest using JPEG.
 *  Returns size in bytes of the compressed data.
 */
static void display ()
{
    uint8_t** decoded;
    int* strides;

    if (decoder_ctx->pix_fmt != PIX_FMT)
    {
        sws_scale (
            sws_ctx,
            (const uint8_t* const*) frame->data,
            frame->linesize,
            0,
            decoder_ctx->height,
            video_dst_data,
            video_dst_linesize
        );
        decoded = video_dst_data;
        strides = video_dst_linesize;
    }
    else
    {
        decoded = frame->data;
        strides = frame->linesize;
    }

    // redo to planar form
    for (int y = 0; y < decoder_ctx->height; y ++)
        for (int x = 0; x < decoder_ctx->width; x ++)
            planar_image[y * decoder_ctx->width * 2 + x * 2 + 1] = decoded[0][y * strides[0] + x];

    for (int y = 0; y < decoder_ctx->height; y ++)
        for (int x = 0; x < decoder_ctx->width / 2; x ++)
            planar_image[y * decoder_ctx->width * 2 + x * 4] = decoded[1][y * strides[1] + x];

    for (int y = 0; y < decoder_ctx->height; y ++)
        for (int x = 0; x < decoder_ctx->width / 2; x ++)
            planar_image[y * decoder_ctx->width * 2 + x * 4 + 2] = decoded[2][y * strides[2] + x];

    // draw the shit
    load_texture (planar_image, decoder_ctx->width, decoder_ctx->height);
    draw_ui ();
}

/**
 *  Check input events from user.
 */
static void check_events ()
{
    int paused = 0;
    do
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
                    case SDLK_RIGHT:
                        seek (100);
                    break;
                    case SDLK_LEFT:
                        seek (0);
                    break;
                    case SDLK_SPACE:
                        paused = !paused;
                    break;
                }
                break;
            }
        }
        usleep (100);
    }
    while (paused);
}

/**
 *  Transcode the stream.
 *  Takes a function pointer to a store function.
 */
static void play ()
{
    stop = 0;
    planar_image = malloc (decoder_ctx->width * decoder_ctx->height * 2);
    int iterations = 0;

    /* for timing decoding */
    struct timespec uno, dos;
    unsigned long int total_duration    = 0;
    unsigned long int shortest_duration = 0xffffffffff;
    unsigned long int longest_duration  = 0;
    unsigned long int duration          = 0;

    int64_t search_pts = 0;
    clock_gettime (CLOCK_MONOTONIC, &uno);
    random_seek (&search_pts);

    // start playing
    while (av_read_frame (fmt_ctx, &packet) >= 0 && !stop)
    {
        AVPacket original = packet;
        if (packet.stream_index != stream_index)
        {
            // audio packet - ignore
            goto loop;
        }
        // printf ("packet pts: %ld\n", packet.dts);

        // decode it
        int ret = 0;
        int got_frame = 0;
        do
        {
            // partially decode
            ret = avcodec_decode_video2 (decoder_ctx, frame, &got_frame, &packet);
            if (ret < 0)
            {
                break;
            }
            // display
            if (got_frame)
            {
                if (packet.pts >= search_pts || packet.dts >= search_pts)
                {
                    clock_gettime (CLOCK_MONOTONIC, &dos);
                    duration = (dos.tv_nsec - uno.tv_nsec) + (dos.tv_sec - uno.tv_sec) * BILLION;
                    total_duration += duration;
                    if (duration > longest_duration)
                        longest_duration = duration;
                    if (duration < shortest_duration)
                        shortest_duration = duration;

                    display ();
                    iterations ++;

                    clock_gettime (CLOCK_MONOTONIC, &uno);
                    random_seek (&search_pts);
                }
                else
                {
                    printf ("%ld != %ld\n", packet.pts, search_pts);
                }
            }
            packet.size -= ret;
            packet.data += ret;
        }
        while (packet.size > 0);
loop:
        av_packet_unref (&original);
        check_events ();
    }
    free (planar_image);

    printf ("\n");
    printf ("results:\n");
    printf ("done decoding %d frames.\n", iterations);
    printf ("  total time:    %6.4f s\n", (double) total_duration / BILLION);
    printf ("  average time:  %6.4f s\n", (double) total_duration / BILLION / iterations);
    printf ("  longest time:  %6.4f s\n", (double) longest_duration / BILLION);
    printf ("  shortest time: %6.4f s\n", (double) shortest_duration / BILLION);
    printf ("  fps:           %6.4f  \n", (double) 1.0 / (total_duration / BILLION / iterations));
    printf ("\n");
}


int main (int argc, char** argv)
{
    av_register_all ();
    init_ui (960, 540);
    if (open (argv[1]) != 0)
    {
        fprintf (stderr, "error opening media file\n");
        return 1;
    };
    play ();
    close ();
    deinit_ui ();
    return 0;
}
