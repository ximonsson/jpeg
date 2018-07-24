#include "jpeg/jpeg.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>


#define TRANSCODE_MULTI_ARG     "multi"
#define TRANSCODE_SINGLE_ARG    "single"
#define MULTIFILES_PATH         "video/multi"
#define SINGLEFILE_PATH         "video/single"
#define PIX_FMT                 AV_PIX_FMT_YUV422P


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

static FILE*                fp;
static FILE*                ifp;


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

    // init jpeg codec
    jpeg_init (decoder_ctx->width, decoder_ctx->height, 0);

    return 0;
}

/**
 *  Close the video source.
 */
static int close ()
{
    av_freep (&video_dst_data[0]);
    return 0;
}

/**
 *  Compress the current frame to dest using JPEG.
 *  Returns size in bytes of the compressed data.
 */
static int compress (uint8_t* dest)
{
    int compressed_size;
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

    // compress
    compressed_size = jpeg_compress (planar_image, dest);

    // draw the shit
    load_texture (planar_image, decoder_ctx->width, decoder_ctx->height);
    draw_ui ();

    return compressed_size;
}

/**
 *  Transcode the stream.
 *  Takes a function pointer to a store function.
 */
static void transcode (int (*store) (uint8_t*, size_t, int))
{
    uint8_t* jpeg_buffer = malloc (decoder_ctx->width * decoder_ctx->height * 2);
    planar_image         = malloc (decoder_ctx->width * decoder_ctx->height * 2);
    int frame_count      = 0;

    // start transcoding
    while (av_read_frame (fmt_ctx, &packet) >= 0 && frame_count < 1500)
    {
        if (packet.stream_index != stream_index)
        {
            av_free_packet (&packet);
            continue;
        }
        // encode and store to file
        // decode it
        AVPacket original = packet;
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
            // store frame to disk
            if (got_frame)
            {
                frame_count ++;
                memset (jpeg_buffer, 0, decoder_ctx->width * decoder_ctx->height * 2);
                int compressed_size = compress (jpeg_buffer);
                if (compressed_size > 0)
                {
                    store (jpeg_buffer, compressed_size, frame_count);
                }
            }
            packet.size -= ret;
            packet.data += ret;
        }
        while (packet.size > 0);
        av_free_packet (&original);
    }

    free (jpeg_buffer);
    free (planar_image);
    jpeg_deinit ();
}


int transcoder_init ()
{
    av_register_all ();
    return 0;
}


void transcoder_deinit ()
{

}


static int store_separate_files (uint8_t* data, size_t size, int frame_count)
{
    char path[256] = { 0 };
    sprintf (path, "%s/%d.jpg", MULTIFILES_PATH, frame_count);
    // write compressed data to file
    fp = fopen (path, "wb");
    fwrite (data, 1, size, fp);
    fclose (fp);
    return 0;
}

static int store_single_file (uint8_t* data, size_t size, int frame_count)
{
    int pos = ftell (fp);
    int s = size;
    // fseek  (ifp, frame_count * sizeof (int) * 2, SEEK_SET);
    fwrite (&pos, 1, sizeof (int), ifp);
    fwrite (&s, 1, sizeof (int), ifp);
    fwrite (data, 1, size, fp);
    return 0;
}


int transcode__separate_files (const char* source)
{
    // open media
    if (open (source) != 0)
        return 1;
    transcode (store_separate_files);
    close ();
    return 0;
}


int transcode__one_file (const char* source)
{
    if (open (source) != 0)
        return 1;
    fp = fopen (SINGLEFILE_PATH"/video.mjpg", "wb");
    ifp = fopen (SINGLEFILE_PATH"/index", "wb");
    transcode (store_single_file);
    fclose (fp);
    fclose (ifp);
    close ();
    return 0;
}


int main (int argc, char** argv)
{
    if (argc < 3)
    {
        printf ("usage: %s <%s|%s> <source>\n", argv[0], TRANSCODE_MULTI_ARG, TRANSCODE_SINGLE_ARG);
        return 1;
    }

    int ret = 0;
    transcoder_init ();
    init_ui (960, 540);

    if (strcmp (argv[1], TRANSCODE_MULTI_ARG) == 0)
    {
        printf ("transcoding source [%s] to multiple files in [%s]... \n", argv[2], MULTIFILES_PATH);
        ret = transcode__separate_files (argv[2]);
    }
    else if (strcmp (argv[1], TRANSCODE_SINGLE_ARG) == 0)
    {
        ret = transcode__one_file (argv[2]);
    }
    printf ("done\n");

    transcoder_deinit ();
    deinit_ui ();
    return ret;
}
