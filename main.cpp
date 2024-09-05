#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

static void encode(AVCodecContext *enc_ctx, AVFormatContext *output_context,
                   AVFrame *frame, AVPacket *pkt) {
    int ret;

    ret = avcodec_send_frame(enc_ctx, frame);
    if (ret < 0) {
        exit(1);
    }

    while (ret >= 0) {
        ret = avcodec_receive_packet(enc_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0) {
            exit(1);
        }

        av_interleaved_write_frame(output_context, pkt);
        av_packet_unref(pkt);
    }
}

int main(int argc, char **argv) {
    avformat_network_init();

    AVFormatContext *output_context = NULL;
    avformat_alloc_output_context2(&output_context, NULL, "flv", NULL);
    if (output_context == NULL) {
        exit(1);
    }

    if (avio_open2(&output_context->pb, "rtmp://192.168.1.2/live/test",
                   AVIO_FLAG_WRITE, nullptr, nullptr) < 0) {
        exit(1);
    }

    auto codec = avcodec_find_encoder_by_name("libx264");
    if (!codec) {
        exit(1);
    }

    auto out_stream = avformat_new_stream(output_context, codec);
    if (out_stream == nullptr) {
        exit(1);
    }

    auto avcodec_context = avcodec_alloc_context3(codec);
    if (!avcodec_context) {
        exit(1);
    }

    avcodec_context->codec_tag = 0;
    avcodec_context->codec_id = AV_CODEC_ID_H264;
    avcodec_context->codec_type = AVMEDIA_TYPE_VIDEO;
    avcodec_context->bit_rate = 400000;
    avcodec_context->width = 352;
    avcodec_context->height = 288;
    avcodec_context->time_base = (AVRational){1, 25};
    avcodec_context->framerate = (AVRational){25, 1};
    avcodec_context->gop_size = 10;
    avcodec_context->max_b_frames = 1;
    avcodec_context->pix_fmt = AV_PIX_FMT_YUV420P;
    avcodec_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    av_opt_set(avcodec_context->priv_data, "preset", "veryfast", 0);
    av_opt_set(avcodec_context->priv_data, "profile", "baseline", 0);
    av_opt_set(avcodec_context->priv_data, "tune", "zerolatency", 0);

    if (avcodec_parameters_from_context(out_stream->codecpar, avcodec_context) <
        0) {
        exit(1);
    }

    if (avcodec_open2(avcodec_context, codec, NULL) < 0) {
        exit(1);
    }

    out_stream->codecpar->extradata = avcodec_context->extradata;
    out_stream->codecpar->extradata_size = avcodec_context->extradata_size;

    if (avformat_write_header(output_context, nullptr) < 0) {
        exit(1);
    }

    auto frame = av_frame_alloc();
    if (!frame) {
        exit(1);
    }
    frame->format = avcodec_context->pix_fmt;
    frame->width = avcodec_context->width;
    frame->height = avcodec_context->height;

    if (av_frame_get_buffer(frame, 0) < 0) {
        exit(1);
    }

    auto pkt = av_packet_alloc();
    if (!pkt) {
        exit(1);
    }

    for (int i = 0; i < 9999999; i++) {
        auto ret = av_frame_make_writable(frame);
        if (ret < 0) {
            exit(1);
        }

        for (int y = 0; y < avcodec_context->height; y++) {
            for (int x = 0; x < avcodec_context->width; x++) {
                frame->data[0][y * frame->linesize[0] + x] = x + y + i * 3;
            }
        }

        for (int y = 0; y < avcodec_context->height / 2; y++) {
            for (int x = 0; x < avcodec_context->width / 2; x++) {
                frame->data[1][y * frame->linesize[1] + x] = 128 + y + i * 2;
                frame->data[2][y * frame->linesize[2] + x] = 64 + x + i * 5;
            }
        }

        frame->pts = i;

        encode(avcodec_context, output_context, frame, pkt);
        usleep(40 * 1000);
    }

    avcodec_free_context(&avcodec_context);
    av_frame_free(&frame);
    av_packet_free(&pkt);

    return 0;
}
