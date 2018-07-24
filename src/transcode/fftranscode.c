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
#define N_FRAMES                500
#define ERR(msg, ret) \
    fprintf (stderr, "%s\n", msg); return ret;


static AVFormatContext*     fmt_ctx;
static AVFormatContext*     ofmt_ctx;
static AVStream*            stream;
static AVStream*            out_stream;
static AVCodecContext*      decoder_ctx;
static AVCodecContext*      encoder_ctx;
static AVCodec*             decoder;
static AVCodec*             encoder;
static AVPacket             packet;
static AVFrame*             frame;
static int                  stream_index;


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
        ERR ("could not find decoder", 1);
    }

    // open codec
    if ((ret = avcodec_open2 (decoder_ctx, decoder, NULL)) < 0)
    {
        ERR ("failed to open video codec", 1);
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

    return 0;
}

/**
 *  Open destination file for writing.
 */
static int open_destination (const char* destination)
{
    avformat_alloc_output_context2 (&ofmt_ctx, NULL, NULL, destination);
    if (!ofmt_ctx)
    {
        ERR ("could not open destination.", 1);
    }

    // AV_CODEC_ID_MJPEG
    // AV_CODEC_ID_DIRAC
    encoder = avcodec_find_encoder (AV_CODEC_ID_VP9);
    if (!encoder)
    {
        ERR ("could not get encoder", 1);
    }

    out_stream = avformat_new_stream (ofmt_ctx, encoder);
    if (!out_stream)
    {
        ERR ("could not add new stream", 1);
    }

    out_stream->time_base            = stream->time_base;
    encoder_ctx                      = out_stream->codec;
    encoder_ctx->height              = decoder_ctx->height;
    encoder_ctx->width               = decoder_ctx->width;
    encoder_ctx->time_base           = decoder_ctx->time_base;
    encoder_ctx->sample_aspect_ratio = decoder_ctx->sample_aspect_ratio;
    encoder_ctx->pix_fmt             = encoder->pix_fmts[0];

    if (avcodec_open2 (encoder_ctx, encoder, NULL) < 0)
    {
        ERR ("could not open encoder", 1);
    }

    if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        encoder_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    av_dump_format (ofmt_ctx, 0, destination, 1);

    if (avio_open (&ofmt_ctx->pb, destination, AVIO_FLAG_WRITE) < 0)
    {
        ERR ("could open file for writing", 1);
    }

    if (avformat_write_header (ofmt_ctx, NULL) < 0)
    {
        ERR ("error writing header", 1);
    }

    return 0;
}

/**
 *  Close the video source.
 */
static int close ()
{
    return 0;
}

/**
 *  Compress the current frame to dest using JPEG.
 *  Returns size in bytes of the compressed data.
 */
static int encode ()
{
    AVPacket pkt;
    av_init_packet (&pkt);
	pkt.data = NULL;
	pkt.size = 0;

    int ret = 0;
    int got_packet = 0;

    if ((ret = avcodec_encode_video2 (encoder_ctx, &pkt, frame, &got_packet)) != 0)
	{
		fprintf (stderr, "could not encode frame\n");
		return ret;
	}
    // write packet to stream
	if (got_packet && pkt.size)
	{
        pkt.stream_index = out_stream->index;
        av_packet_rescale_ts (&pkt, out_stream->codec->time_base, out_stream->time_base);
        ret = av_interleaved_write_frame (ofmt_ctx, &pkt);
	}

    av_free (pkt.data);
	av_free_packet (&pkt);
    return ret;
}

/**
 *  Transcode the stream.
 *  Takes a function pointer to a store function.
 */
static void transcode ()
{
    int frame_count = 0;

    // start transcoding
    while (av_read_frame (fmt_ctx, &packet) >= 0 && frame_count < N_FRAMES)
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
                encode ();
                frame_count ++;
            }
            packet.size -= ret;
            packet.data += ret;
        }
        while (packet.size > 0);
        av_free_packet (&original);
    }

    av_write_trailer (ofmt_ctx);
}


int transcoder_init ()
{
    av_register_all ();
    return 0;
}


void transcoder_deinit ()
{

}


int main (int argc, char** argv)
{
    if (argc < 3)
    {
        printf ("usage: %s <source> <destination>\n", argv[0]);
        return 1;
    }

    int ret = 0;
    transcoder_init ();

    if (open (argv[1]) != 0 || open_destination (argv[2]) != 0)
    {
        return 1;
    }

    transcode ();
    close ();
    transcoder_deinit ();
    return ret;
}
