//
// Created by Cethric on 7/12/2017.
//

#include "render/surface/VideoDecoder.h"

typedef struct CodecOpaqueStruct {
    AVPixelFormat hwPixexlFormat;
} CodecOpaqueStruct;

class BufferData {
private:
    GLubyte *buffer;
    size_t length;
    GLsizei width;
    GLsizei height;
    bool ready;

public:
    BufferData() {
        ready = false;
        buffer = nullptr;
        length = 0;
        width = 0;
        height = 0;
    }

    ~BufferData() {
        delete[] buffer;
    }

    void ReadyBuffer(GLsizei width, GLsizei height, const uint8_t *copy, int size) {
        this->width = width;
        this->height = height;
        if (this->length != size * sizeof(GLubyte)) {
            wxPrintf("BufferData has changed size from %llu to %llu\n", this->length, size * sizeof(GLubyte));
            delete[] this->buffer;
            this->length = size * sizeof(GLubyte);
            this->buffer = new GLubyte[this->length];
        }
        for (size_t i = 0; i < this->length; i++) {
            this->buffer[i] = copy[i];
        }
        ready = true;
    }

    GLsizei GetWidth() {
        return width;
    }

    GLsizei GetHeight() {
        return height;
    }

    GLubyte *GetBuffer() {
        return buffer;
    }

    bool IsReady() {
        return ready;
    }

    void Reset() {
        ready = false;
    }

};


class DecoderThread : public wxThread {
    friend DT::Presenter::Render::Surface::VideoDecoder;
private:
    DT::Presenter::Render::Surface::VideoDecoder *decoder;


    AVFrame *yuvFrame = nullptr;
    AVFrame *swFrame = nullptr;

    AVPacket *packet = nullptr;
    struct SwsContext *swsContext = nullptr;
    int numBytes;
    int ret;

    uint8_t *rgbBuffer[AV_NUM_DATA_POINTERS];
    int rgbLines[AV_NUM_DATA_POINTERS];

public:
    explicit DecoderThread(DT::Presenter::Render::Surface::VideoDecoder *decoder) : wxThread(wxTHREAD_JOINABLE) {
        this->decoder = decoder;
        numBytes = 0;
        ret = 0;

        for (auto &i : rgbLines) {
            i = 0;
        }
        for (auto &i : rgbBuffer) {
            i = nullptr;
        }
    }

    wxThread::ExitCode Entry() wxOVERRIDE {
        AVFrame *hwFrame = nullptr;

        AVPixelFormat outputFormat = AV_PIX_FMT_RGB24;

        numBytes = av_image_get_buffer_size(
                outputFormat, decoder->codecContext->width,
                decoder->codecContext->height, 1
        );
        swsContext = sws_getContext(
                decoder->codecContext->width, decoder->codecContext->height,
                decoder->codecContext->pix_fmt, decoder->codecContext->width,
                decoder->codecContext->height, outputFormat, SWS_BICUBLIN, nullptr,
                nullptr, nullptr
        );

        decoder->running = true;

        rgbLines[0] = 1920 * 3;
        rgbBuffer[0] = (uint8_t *) av_malloc(numBytes + 16 + sizeof(uint8_t));

        if ((packet = av_packet_alloc()) == nullptr) {
            return reinterpret_cast<wxThread::ExitCode>(-1);
        }

        if ((yuvFrame = av_frame_alloc()) == nullptr) {
            return reinterpret_cast<wxThread::ExitCode>(-1);
        }

        if ((swFrame = av_frame_alloc()) == nullptr) {
            return reinterpret_cast<wxThread::ExitCode>(-1);
        }

        int last_frame_decoded = -1;

        while (av_read_frame(decoder->formatContext, packet) >= 0 && !TestDestroy()) {
            if (packet->stream_index == decoder->videoStreamIndex) {
                ret = avcodec_send_packet(decoder->codecContext, packet);
                if (ret < 0) {
                    char errorBuff[AV_ERROR_MAX_STRING_SIZE];
                    int r = av_strerror(ret, &errorBuff[0], AV_ERROR_MAX_STRING_SIZE);
                    wxPrintf("Failed to send packet: %d %s\n", r == 0 ? ret : r, &errorBuff[0]);
                    return reinterpret_cast<wxThread::ExitCode>(ret);
                }

                while (ret >= 0 && !TestDestroy()) {

                    av_frame_unref(yuvFrame);

                    ret = avcodec_receive_frame(decoder->codecContext, yuvFrame);

                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                        av_packet_unref(packet);
                        break;

                    } else if (ret < 0) {
                        return reinterpret_cast<wxThread::ExitCode>(ret);
                    } else {
                        while (decoder->frameDataBuffer->IsReady() && !TestDestroy()) {
                            wxYield();
                        }

                        if (TestDestroy()) {
                            return reinterpret_cast<wxThread::ExitCode>(0);
                        }

                        if (yuvFrame->format ==
                            ((CodecOpaqueStruct *) this->decoder->codecContext->opaque)->hwPixexlFormat) {
                            av_frame_unref(swFrame);
                            if (av_hwframe_transfer_data(swFrame, yuvFrame, 0) < 0) {
                                wxPrintf("Failed hw decode\n");
                                hwFrame = yuvFrame;
                            } else {
                                if (av_frame_copy_props(swFrame, yuvFrame) < 0) {
                                    wxPrintf("Could not copy properties\n");
                                }
                                hwFrame = swFrame;
                            }
                        } else {
                            hwFrame = yuvFrame;
                        }

                        if (TestDestroy()) {
                            return reinterpret_cast<wxThread::ExitCode>(0);
                        }

                        // copy frame
                        if (hwFrame->coded_picture_number != last_frame_decoded) {
                            swsContext = sws_getCachedContext(swsContext, hwFrame->width, hwFrame->height,
                                                              static_cast<AVPixelFormat>(hwFrame->format),
                                                              decoder->codecContext->width,
                                                              decoder->codecContext->height, outputFormat,
                                                              SWS_FAST_BILINEAR, nullptr,
                                                              nullptr, nullptr);

                            sws_scale(
                                    swsContext, reinterpret_cast<const uint8_t *const *>(hwFrame->data),
                                    hwFrame->linesize, 0, decoder->codecContext->height,
                                    rgbBuffer, rgbLines
                            );


                            decoder->frameDataBuffer->ReadyBuffer(hwFrame->width, hwFrame->height, rgbBuffer[0],
                                                                  numBytes);

                            last_frame_decoded = hwFrame->coded_picture_number;

                        }
                    }
                }
            }
            av_packet_unref(packet);
        }

        return reinterpret_cast<wxThread::ExitCode>(0);
    }

    void OnExit() wxOVERRIDE {
        av_frame_unref(yuvFrame);
        av_frame_free(&yuvFrame);
        av_frame_free(&swFrame);
        decoder->frameDataBuffer->Reset();
        av_packet_free(&packet);
        sws_freeContext(swsContext);
        av_free(rgbBuffer[0]);

        // Video has finished exit thread

        decoder->running = false;
        decoder = nullptr;
        wxPrintf("VideoThread has completed\n");
    }
};

DT::Presenter::Render::Surface::VideoDecoder::VideoDecoder() {
    static bool setup;
    if (!setup) {

        av_log_set_flags(AV_LOG_PRINT_LEVEL);
//        av_log_set_level(AV_LOG_TRACE);

        avcodec_register_all();
        avdevice_register_all();
        avfilter_register_all();
        av_register_all();

        AVOutputFormat *outputFormat = nullptr;
        AVCodec *codecs = nullptr;
        AVHWDeviceType hwDeviceType = AV_HWDEVICE_TYPE_NONE;
        const AVFilter *filter = nullptr;

        wxPrintf("Available Codecs:\n");
        while ((codecs = av_codec_next(codecs)) != nullptr) {
            wxPrintf("    %s: %s\n", codecs->name, codecs->long_name);
        }

        wxPrintf("Available Output Devices:\n");
        while ((outputFormat = av_output_video_device_next(outputFormat)) != nullptr) {
            wxPrintf("    %s: ", outputFormat->name);
            wxPrintf("    %s <%s> (%s)\n", outputFormat->long_name, outputFormat->mime_type, outputFormat->extensions);
        }

        wxPrintf("Available Hardware Decoders\n");
        while ((hwDeviceType = av_hwdevice_iterate_types(hwDeviceType)) != AV_HWDEVICE_TYPE_NONE) {
            wxPrintf("    %s\n", av_hwdevice_get_type_name(hwDeviceType));
        }

        wxPrintf("Available Filters\n");
        while ((filter = avfilter_next(filter)) != nullptr) {
            wxPrintf("    %s: ", filter->name);
            wxPrintf("    %s\n", filter->description);
        }

        setup = true;

    }
    this->playbackStyle = DT::Presenter::Render::Surface::VIDEO_PLAY_FORWARD;
}

DT::Presenter::Render::Surface::VideoDecoder::~VideoDecoder() {
    this->Stop();
    delete (CodecOpaqueStruct *) this->codecContext->opaque;
    this->codecContext->opaque = nullptr;
    av_buffer_unref(&this->codecContext->hw_frames_ctx);
    av_buffer_unref(&this->codecContext->hw_device_ctx);
    avcodec_free_context(&this->codecContext);
    av_buffer_unref(&hwDeviceContext);
    av_free(hwDeviceContext);
    avformat_free_context(this->formatContext);

    delete this->frameDataBuffer;
    this->frameDataBuffer = nullptr;
    wxPrintf("Video Decoder destroyed\n");
}

static wxString pixelFormatToString(AVPixelFormat format) {
    switch (format) {
        case AV_PIX_FMT_NONE:
            return "AV_PIX_FMT_NONE";
        case AV_PIX_FMT_YUV420P:
            return "AV_PIX_FMT_YUV420P";
        case AV_PIX_FMT_YUYV422:
            return "AV_PIX_FMT_YUYV422";
        case AV_PIX_FMT_RGB24:
            return "AV_PIX_FMT_RGB24";
        case AV_PIX_FMT_BGR24:
            return "AV_PIX_FMT_BGR24";
        case AV_PIX_FMT_YUV422P:
            return "AV_PIX_FMT_YUV422P";
        case AV_PIX_FMT_YUV444P:
            return "AV_PIX_FMT_YUV444P";
        case AV_PIX_FMT_YUV410P:
            return "AV_PIX_FMT_YUV410P";
        case AV_PIX_FMT_YUV411P:
            return "AV_PIX_FMT_YUV411P";
        case AV_PIX_FMT_GRAY8:
            return "AV_PIX_FMT_GRAY8";
        case AV_PIX_FMT_MONOWHITE:
            return "AV_PIX_FMT_MONOWHITE";
        case AV_PIX_FMT_MONOBLACK:
            return "AV_PIX_FMT_MONOBLACK";
        case AV_PIX_FMT_PAL8:
            return "AV_PIX_FMT_PAL8";
        case AV_PIX_FMT_YUVJ420P:
            return "AV_PIX_FMT_YUVJ420P";
        case AV_PIX_FMT_YUVJ422P:
            return "AV_PIX_FMT_YUVJ422P";
        case AV_PIX_FMT_YUVJ444P:
            return "AV_PIX_FMT_YUVJ444P";
        case AV_PIX_FMT_UYVY422:
            return "AV_PIX_FMT_UYVY422";
        case AV_PIX_FMT_UYYVYY411:
            return "AV_PIX_FMT_UYYVYY411";
        case AV_PIX_FMT_BGR8:
            return "AV_PIX_FMT_BGR8";
        case AV_PIX_FMT_BGR4:
            return "AV_PIX_FMT_BGR4";
        case AV_PIX_FMT_BGR4_BYTE:
            return "AV_PIX_FMT_BGR4_BYTE";
        case AV_PIX_FMT_RGB8:
            return "AV_PIX_FMT_RGB8";
        case AV_PIX_FMT_RGB4:
            return "AV_PIX_FMT_RGB4";
        case AV_PIX_FMT_RGB4_BYTE:
            return "AV_PIX_FMT_RGB4_BYTE";
        case AV_PIX_FMT_NV12:
            return "AV_PIX_FMT_NV12";
        case AV_PIX_FMT_NV21:
            return "AV_PIX_FMT_NV21";
        case AV_PIX_FMT_ARGB:
            return "AV_PIX_FMT_ARGB";
        case AV_PIX_FMT_RGBA:
            return "AV_PIX_FMT_RGBA";
        case AV_PIX_FMT_ABGR:
            return "AV_PIX_FMT_ABGR";
        case AV_PIX_FMT_BGRA:
            return "AV_PIX_FMT_BGRA";
        case AV_PIX_FMT_GRAY16BE:
            return "AV_PIX_FMT_GRAY16BE";
        case AV_PIX_FMT_GRAY16LE:
            return "AV_PIX_FMT_GRAY16LE";
        case AV_PIX_FMT_YUV440P:
            return "AV_PIX_FMT_YUV440P";
        case AV_PIX_FMT_YUVJ440P:
            return "AV_PIX_FMT_YUVJ440P";
        case AV_PIX_FMT_YUVA420P:
            return "AV_PIX_FMT_YUVA420P";
        case AV_PIX_FMT_RGB48BE:
            return "AV_PIX_FMT_RGB48BE";
        case AV_PIX_FMT_RGB48LE:
            return "AV_PIX_FMT_RGB48LE";
        case AV_PIX_FMT_RGB565BE:
            return "AV_PIX_FMT_RGB565BE";
        case AV_PIX_FMT_RGB565LE:
            return "AV_PIX_FMT_RGB565LE";
        case AV_PIX_FMT_RGB555BE:
            return "AV_PIX_FMT_RGB555BE";
        case AV_PIX_FMT_RGB555LE:
            return "AV_PIX_FMT_RGB555LE";
        case AV_PIX_FMT_BGR565BE:
            return "AV_PIX_FMT_BGR565BE";
        case AV_PIX_FMT_BGR565LE:
            return "AV_PIX_FMT_BGR565LE";
        case AV_PIX_FMT_BGR555BE:
            return "AV_PIX_FMT_BGR555BE";
        case AV_PIX_FMT_BGR555LE:
            return "AV_PIX_FMT_BGR555LE";
        case AV_PIX_FMT_VAAPI_MOCO:
            return "AV_PIX_FMT_VAAPI_MOCO";
        case AV_PIX_FMT_VAAPI_IDCT:
            return "AV_PIX_FMT_VAAPI_IDCT";
        case AV_PIX_FMT_VAAPI_VLD:
            return "AV_PIX_FMT_VAAPI_VLD";
        case AV_PIX_FMT_YUV420P16LE:
            return "AV_PIX_FMT_YUV420P16LE";
        case AV_PIX_FMT_YUV420P16BE:
            return "AV_PIX_FMT_YUV420P16BE";
        case AV_PIX_FMT_YUV422P16LE:
            return "AV_PIX_FMT_YUV422P16LE";
        case AV_PIX_FMT_YUV422P16BE:
            return "AV_PIX_FMT_YUV422P16BE";
        case AV_PIX_FMT_YUV444P16LE:
            return "AV_PIX_FMT_YUV444P16LE";
        case AV_PIX_FMT_YUV444P16BE:
            return "AV_PIX_FMT_YUV444P16BE";
        case AV_PIX_FMT_DXVA2_VLD:
            return "AV_PIX_FMT_DXVA2_VLD";
        case AV_PIX_FMT_RGB444LE:
            return "AV_PIX_FMT_RGB444LE";
        case AV_PIX_FMT_RGB444BE:
            return "AV_PIX_FMT_RGB444BE";
        case AV_PIX_FMT_BGR444LE:
            return "AV_PIX_FMT_BGR444LE";
        case AV_PIX_FMT_BGR444BE:
            return "AV_PIX_FMT_BGR444BE";
        case AV_PIX_FMT_YA8:
            return "AV_PIX_FMT_YA8";
        case AV_PIX_FMT_BGR48BE:
            return "AV_PIX_FMT_BGR48BE";
        case AV_PIX_FMT_BGR48LE:
            return "AV_PIX_FMT_BGR48LE";
        case AV_PIX_FMT_YUV420P9BE:
            return "AV_PIX_FMT_YUV420P9BE";
        case AV_PIX_FMT_YUV420P9LE:
            return "AV_PIX_FMT_YUV420P9LE";
        case AV_PIX_FMT_YUV420P10BE:
            return "AV_PIX_FMT_YUV420P10BE";
        case AV_PIX_FMT_YUV420P10LE:
            return "AV_PIX_FMT_YUV420P10LE";
        case AV_PIX_FMT_YUV422P10BE:
            return "AV_PIX_FMT_YUV422P10BE";
        case AV_PIX_FMT_YUV422P10LE:
            return "AV_PIX_FMT_YUV422P10LE";
        case AV_PIX_FMT_YUV444P9BE:
            return "AV_PIX_FMT_YUV444P9BE";
        case AV_PIX_FMT_YUV444P9LE:
            return "AV_PIX_FMT_YUV444P9LE";
        case AV_PIX_FMT_YUV444P10BE:
            return "AV_PIX_FMT_YUV444P10BE";
        case AV_PIX_FMT_YUV444P10LE:
            return "AV_PIX_FMT_YUV444P10LE";
        case AV_PIX_FMT_YUV422P9BE:
            return "AV_PIX_FMT_YUV422P9BE";
        case AV_PIX_FMT_YUV422P9LE:
            return "AV_PIX_FMT_YUV422P9LE";
        case AV_PIX_FMT_GBRP:
            return "AV_PIX_FMT_GBRP";
        case AV_PIX_FMT_GBRP9BE:
            return "AV_PIX_FMT_GBRP9BE";
        case AV_PIX_FMT_GBRP9LE:
            return "AV_PIX_FMT_GBRP9LE";
        case AV_PIX_FMT_GBRP10BE:
            return "AV_PIX_FMT_GBRP10BE";
        case AV_PIX_FMT_GBRP10LE:
            return "AV_PIX_FMT_GBRP10LE";
        case AV_PIX_FMT_GBRP16BE:
            return "AV_PIX_FMT_GBRP16BE";
        case AV_PIX_FMT_GBRP16LE:
            return "AV_PIX_FMT_GBRP16LE";
        case AV_PIX_FMT_YUVA422P:
            return "AV_PIX_FMT_YUVA422P";
        case AV_PIX_FMT_YUVA444P:
            return "AV_PIX_FMT_YUVA444P";
        case AV_PIX_FMT_YUVA420P9BE:
            return "AV_PIX_FMT_YUVA420P9BE";
        case AV_PIX_FMT_YUVA420P9LE:
            return "AV_PIX_FMT_YUVA420P9LE";
        case AV_PIX_FMT_YUVA422P9BE:
            return "AV_PIX_FMT_YUVA422P9BE";
        case AV_PIX_FMT_YUVA422P9LE:
            return "AV_PIX_FMT_YUVA422P9LE";
        case AV_PIX_FMT_YUVA444P9BE:
            return "AV_PIX_FMT_YUVA444P9BE";
        case AV_PIX_FMT_YUVA444P9LE:
            return "AV_PIX_FMT_YUVA444P9LE";
        case AV_PIX_FMT_YUVA420P10BE:
            return "AV_PIX_FMT_YUVA420P10BE";
        case AV_PIX_FMT_YUVA420P10LE:
            return "AV_PIX_FMT_YUVA420P10LE";
        case AV_PIX_FMT_YUVA422P10BE:
            return "AV_PIX_FMT_YUVA422P10BE";
        case AV_PIX_FMT_YUVA422P10LE:
            return "AV_PIX_FMT_YUVA422P10LE";
        case AV_PIX_FMT_YUVA444P10BE:
            return "AV_PIX_FMT_YUVA444P10BE";
        case AV_PIX_FMT_YUVA444P10LE:
            return "AV_PIX_FMT_YUVA444P10LE";
        case AV_PIX_FMT_YUVA420P16BE:
            return "AV_PIX_FMT_YUVA420P16BE";
        case AV_PIX_FMT_YUVA420P16LE:
            return "AV_PIX_FMT_YUVA420P16LE";
        case AV_PIX_FMT_YUVA422P16BE:
            return "AV_PIX_FMT_YUVA422P16BE";
        case AV_PIX_FMT_YUVA422P16LE:
            return "AV_PIX_FMT_YUVA422P16LE";
        case AV_PIX_FMT_YUVA444P16BE:
            return "AV_PIX_FMT_YUVA444P16BE";
        case AV_PIX_FMT_YUVA444P16LE:
            return "AV_PIX_FMT_YUVA444P16LE";
        case AV_PIX_FMT_VDPAU:
            return "AV_PIX_FMT_VDPAU";
        case AV_PIX_FMT_XYZ12LE:
            return "AV_PIX_FMT_XYZ12LE";
        case AV_PIX_FMT_XYZ12BE:
            return "AV_PIX_FMT_XYZ12BE";
        case AV_PIX_FMT_NV16:
            return "AV_PIX_FMT_NV16";
        case AV_PIX_FMT_NV20LE:
            return "AV_PIX_FMT_NV20LE";
        case AV_PIX_FMT_NV20BE:
            return "AV_PIX_FMT_NV20BE";
        case AV_PIX_FMT_RGBA64BE:
            return "AV_PIX_FMT_RGBA64BE";
        case AV_PIX_FMT_RGBA64LE:
            return "AV_PIX_FMT_RGBA64LE";
        case AV_PIX_FMT_BGRA64BE:
            return "AV_PIX_FMT_BGRA64BE";
        case AV_PIX_FMT_BGRA64LE:
            return "AV_PIX_FMT_BGRA64LE";
        case AV_PIX_FMT_YVYU422:
            return "AV_PIX_FMT_YVYU422";
        case AV_PIX_FMT_YA16BE:
            return "AV_PIX_FMT_YA16BE";
        case AV_PIX_FMT_YA16LE:
            return "AV_PIX_FMT_YA16LE";
        case AV_PIX_FMT_GBRAP:
            return "AV_PIX_FMT_GBRAP";
        case AV_PIX_FMT_GBRAP16BE:
            return "AV_PIX_FMT_GBRAP16BE";
        case AV_PIX_FMT_GBRAP16LE:
            return "AV_PIX_FMT_GBRAP16LE";
        case AV_PIX_FMT_QSV:
            return "AV_PIX_FMT_QSV";
        case AV_PIX_FMT_MMAL:
            return "AV_PIX_FMT_MMAL";
        case AV_PIX_FMT_D3D11VA_VLD:
            return "AV_PIX_FMT_D3D11VA_VLD";
        case AV_PIX_FMT_CUDA:
            return "AV_PIX_FMT_CUDA";
        case AV_PIX_FMT_0RGB:
            return "AV_PIX_FMT_0RGB";
        case AV_PIX_FMT_RGB0:
            return "AV_PIX_FMT_RGB0";
        case AV_PIX_FMT_0BGR:
            return "AV_PIX_FMT_0BGR";
        case AV_PIX_FMT_BGR0:
            return "AV_PIX_FMT_BGR0";
        case AV_PIX_FMT_YUV420P12BE:
            return "AV_PIX_FMT_YUV420P12BE";
        case AV_PIX_FMT_YUV420P12LE:
            return "AV_PIX_FMT_YUV420P12LE";
        case AV_PIX_FMT_YUV420P14BE:
            return "AV_PIX_FMT_YUV420P14BE";
        case AV_PIX_FMT_YUV420P14LE:
            return "AV_PIX_FMT_YUV420P14LE";
        case AV_PIX_FMT_YUV422P12BE:
            return "AV_PIX_FMT_YUV422P12BE";
        case AV_PIX_FMT_YUV422P12LE:
            return "AV_PIX_FMT_YUV422P12LE";
        case AV_PIX_FMT_YUV422P14BE:
            return "AV_PIX_FMT_YUV422P14BE";
        case AV_PIX_FMT_YUV422P14LE:
            return "AV_PIX_FMT_YUV422P14LE";
        case AV_PIX_FMT_YUV444P12BE:
            return "AV_PIX_FMT_YUV444P12BE";
        case AV_PIX_FMT_YUV444P12LE:
            return "AV_PIX_FMT_YUV444P12LE";
        case AV_PIX_FMT_YUV444P14BE:
            return "AV_PIX_FMT_YUV444P14BE";
        case AV_PIX_FMT_YUV444P14LE:
            return "AV_PIX_FMT_YUV444P14LE";
        case AV_PIX_FMT_GBRP12BE:
            return "AV_PIX_FMT_GBRP12BE";
        case AV_PIX_FMT_GBRP12LE:
            return "AV_PIX_FMT_GBRP12LE";
        case AV_PIX_FMT_GBRP14BE:
            return "AV_PIX_FMT_GBRP14BE";
        case AV_PIX_FMT_GBRP14LE:
            return "AV_PIX_FMT_GBRP14LE";
        case AV_PIX_FMT_YUVJ411P:
            return "AV_PIX_FMT_YUVJ411P";
        case AV_PIX_FMT_BAYER_BGGR8:
            return "AV_PIX_FMT_BAYER_BGGR8";
        case AV_PIX_FMT_BAYER_RGGB8:
            return "AV_PIX_FMT_BAYER_RGGB8";
        case AV_PIX_FMT_BAYER_GBRG8:
            return "AV_PIX_FMT_BAYER_GBRG8";
        case AV_PIX_FMT_BAYER_GRBG8:
            return "AV_PIX_FMT_BAYER_GRBG8";
        case AV_PIX_FMT_BAYER_BGGR16LE:
            return "AV_PIX_FMT_BAYER_BGGR16LE";
        case AV_PIX_FMT_BAYER_BGGR16BE:
            return "AV_PIX_FMT_BAYER_BGGR16BE";
        case AV_PIX_FMT_BAYER_RGGB16LE:
            return "AV_PIX_FMT_BAYER_RGGB16LE";
        case AV_PIX_FMT_BAYER_RGGB16BE:
            return "AV_PIX_FMT_BAYER_RGGB16BE";
        case AV_PIX_FMT_BAYER_GBRG16LE:
            return "AV_PIX_FMT_BAYER_GBRG16LE";
        case AV_PIX_FMT_BAYER_GBRG16BE:
            return "AV_PIX_FMT_BAYER_GBRG16BE";
        case AV_PIX_FMT_BAYER_GRBG16LE:
            return "AV_PIX_FMT_BAYER_GRBG16LE";
        case AV_PIX_FMT_BAYER_GRBG16BE:
            return "AV_PIX_FMT_BAYER_GRBG16BE";
        case AV_PIX_FMT_XVMC:
            return "AV_PIX_FMT_XVMC";
        case AV_PIX_FMT_YUV440P10LE:
            return "AV_PIX_FMT_YUV440P10LE";
        case AV_PIX_FMT_YUV440P10BE:
            return "AV_PIX_FMT_YUV440P10BE";
        case AV_PIX_FMT_YUV440P12LE:
            return "AV_PIX_FMT_YUV440P12LE";
        case AV_PIX_FMT_YUV440P12BE:
            return "AV_PIX_FMT_YUV440P12BE";
        case AV_PIX_FMT_AYUV64LE:
            return "AV_PIX_FMT_AYUV64LE";
        case AV_PIX_FMT_AYUV64BE:
            return "AV_PIX_FMT_AYUV64BE";
        case AV_PIX_FMT_VIDEOTOOLBOX:
            return "AV_PIX_FMT_VIDEOTOOLBOX";
        case AV_PIX_FMT_P010LE:
            return "AV_PIX_FMT_P010LE";
        case AV_PIX_FMT_P010BE:
            return "AV_PIX_FMT_P010BE";
        case AV_PIX_FMT_GBRAP12BE:
            return "AV_PIX_FMT_GBRAP12BE";
        case AV_PIX_FMT_GBRAP12LE:
            return "AV_PIX_FMT_GBRAP12LE";
        case AV_PIX_FMT_GBRAP10BE:
            return "AV_PIX_FMT_GBRAP10BE";
        case AV_PIX_FMT_GBRAP10LE:
            return "AV_PIX_FMT_GBRAP10LE";
        case AV_PIX_FMT_MEDIACODEC:
            return "AV_PIX_FMT_MEDIACODEC";
        case AV_PIX_FMT_GRAY12BE:
            return "AV_PIX_FMT_GRAY12BE";
        case AV_PIX_FMT_GRAY12LE:
            return "AV_PIX_FMT_GRAY12LE";
        case AV_PIX_FMT_GRAY10BE:
            return "AV_PIX_FMT_GRAY10BE";
        case AV_PIX_FMT_GRAY10LE:
            return "AV_PIX_FMT_GRAY10LE";
        case AV_PIX_FMT_P016LE:
            return "AV_PIX_FMT_P016LE";
        case AV_PIX_FMT_P016BE:
            return "AV_PIX_FMT_P016BE";
        case AV_PIX_FMT_D3D11:
            return "AV_PIX_FMT_D3D11";
        case AV_PIX_FMT_GRAY9BE:
            return "AV_PIX_FMT_GRAY9BE";
        case AV_PIX_FMT_GRAY9LE:
            return "AV_PIX_FMT_GRAY9LE";
        case AV_PIX_FMT_GBRPF32BE:
            return "AV_PIX_FMT_GBRPF32BE";
        case AV_PIX_FMT_GBRPF32LE:
            return "AV_PIX_FMT_GBRPF32LE";
        case AV_PIX_FMT_GBRAPF32BE:
            return "AV_PIX_FMT_GBRAPF32BE";
        case AV_PIX_FMT_GBRAPF32LE:
            return "AV_PIX_FMT_GBRAPF32LE";
        case AV_PIX_FMT_DRM_PRIME:
            return "AV_PIX_FMT_DRM_PRIME";
        case AV_PIX_FMT_OPENCL:
            return "AV_PIX_FMT_OPENCL";
        default:
            return wxString::Format("Unknown Format %d", format);
    }
}

static enum AVPixelFormat get_hw_format(AVCodecContext *ctx, const enum AVPixelFormat *formats) {
    const enum AVPixelFormat *p;
    wxPrintf("Available Formats: \n");
    for (p = formats; *p != -1; p++) {
        wxPrintf("    %s\n", pixelFormatToString(*p));
    }

    for (p = formats; *p != -1; p++) {
        if (*p == ((CodecOpaqueStruct *) ctx->opaque)->hwPixexlFormat) {
            wxPrintf("Selected Pixel Format %s\n", pixelFormatToString(*p));
//            if (*p == AV_PIX_FMT_D3D11) {
//                if (ctx->hw_device_ctx != nullptr) {
//                    AVBufferRef* hwFrameCtx;
//                    if (avcodec_get_hw_frames_parameters(ctx, ctx->hw_device_ctx, *p, &hwFrameCtx) < 0) {
//                        wxPrintf("Failed to alloc hw frame context\n");
//                        return *p;
//                    }
//
//                    if (av_hwframe_ctx_init(hwFrameCtx) < 0) {
//                        wxPrintf("Failed to init hw frame context\n");
//                        return *p;
//                    }
//                    ctx->hw_frames_ctx = av_buffer_ref(hwFrameCtx);
//                }
//            }
            return *p;
        }
    }
    return AV_PIX_FMT_NONE;
}

bool DT::Presenter::Render::Surface::VideoDecoder::LoadVideo(const wxString &name) {
    if (!wxFileExists(name)) {
        return false;
    }

    int result;
    result = avformat_open_input(&formatContext, name.c_str().AsChar(), nullptr, nullptr);
    if (result < 0) {
        char errorBuff[AV_ERROR_MAX_STRING_SIZE];
        int r = av_strerror(result, &errorBuff[0], AV_ERROR_MAX_STRING_SIZE);
        wxPrintf("Failed to load video: %d %s\n", r == 0 ? result : r, &errorBuff[0]);
        return false;
    }

    result = avformat_find_stream_info(formatContext, nullptr);
    if (result < 0) {
        char errorBuff[AV_ERROR_MAX_STRING_SIZE];
        int r = av_strerror(result, &errorBuff[0], AV_ERROR_MAX_STRING_SIZE);
        wxPrintf("Failed to open stream: %d %s\n", r == 0 ? result : r, &errorBuff[0]);
        return false;
    }

    int stream = av_find_best_stream(formatContext, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
    if (stream < 0) {
        wxPrintf(
                "Failed to find video stream %s\n",
                stream == AVERROR_DECODER_NOT_FOUND ? "Failed to find decoder" : stream == AVERROR_STREAM_NOT_FOUND
                                                                                 ? "Stream not found" : "Unknown"
        );
        return false;
    }
    videoStreamIndex = stream;

    if (!codec) {
        wxPrintf("Failed to find codec\n");
        return false;
    }
    codecContext = avcodec_alloc_context3(codec);

    result = avcodec_parameters_to_context(codecContext, formatContext->streams[videoStreamIndex]->codecpar);
    if (result < 0) {
        char errorBuff[AV_ERROR_MAX_STRING_SIZE];
        int r = av_strerror(result, &errorBuff[0], AV_ERROR_MAX_STRING_SIZE);
        wxPrintf("Failed to copy codec parameters to codec: %d %s\n", r == 0 ? result : r, &errorBuff[0]);
        return false;
    }

    AVPixelFormat hwPixelFormat = AV_PIX_FMT_NONE;
    AVHWDeviceType hwDeviceType = AV_HWDEVICE_TYPE_NONE;
    int methods = 0;

    for (int hwConfigIndex = 0;; hwConfigIndex++) {
        const AVCodecHWConfig *config = avcodec_get_hw_config(codec, hwConfigIndex);
        if (!config) {
            break;
        }
        if (config->methods & AV_CODEC_HW_CONFIG_METHOD_AD_HOC) {
            continue;
        }
        if (!(config->methods & AV_CODEC_HW_CONFIG_METHOD_INTERNAL)) {
            // Second Options
            methods = config->methods;
            hwPixelFormat = config->pix_fmt;
            hwDeviceType = config->device_type;
            break;
        }
    }
    CodecOpaqueStruct *opaqueStruct = new CodecOpaqueStruct();
    codecContext->opaque = opaqueStruct;
    opaqueStruct->hwPixexlFormat = hwPixelFormat;
    av_opt_set_int(codecContext, "refcounted_frames", 1, 0);
    av_opt_set_int(codecContext, "thread_safe_callbacks", 1, 0);

    if (hwDeviceType != AV_HWDEVICE_TYPE_NONE) {
        if (methods & AV_CODEC_HW_CONFIG_METHOD_HW_FRAMES_CTX) {
            codecContext->get_format = get_hw_format;
        }
        if (methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) {
            if (av_hwdevice_ctx_create(&hwDeviceContext, hwDeviceType, nullptr, nullptr, 0) < 0) {
                wxPrintf("Failed to create hardware context\n");
                return false;
            }

            wxPrintf("Using Hardware Decoder %s\n", av_hwdevice_get_type_name(hwDeviceType));
            wxPrintf("Using Pixel Format %s\n", pixelFormatToString(hwPixelFormat));
            if (av_hwdevice_ctx_init(hwDeviceContext) < 0) {
                wxPrintf("Failed to initialise Hardware context\n");
                return false;
            }
            codecContext->hw_device_ctx = av_buffer_ref(hwDeviceContext);
        }
    }


    // TODO Possible memory leak?

    this->codecContext->hwaccel_flags = AV_HWACCEL_FLAG_IGNORE_LEVEL/* | AV_HWACCEL_FLAG_ALLOW_PROFILE_MISMATCH*/;


    this->codecContext->thread_count = wxThread::GetCPUCount() * 2;
    this->codecContext->thread_type = FF_THREAD_FRAME;

    result = avcodec_open2(codecContext, codec, nullptr);
    if (result < 0) {
        char errorBuff[AV_ERROR_MAX_STRING_SIZE];
        int r = av_strerror(result, &errorBuff[0], AV_ERROR_MAX_STRING_SIZE);
        wxPrintf("Failed to open codec: %d %s\n", r == 0 ? result : r, &errorBuff[0]);
        return false;
    }

    return true;
}

void DT::Presenter::Render::Surface::VideoDecoder::SetPlaybackStyle(
        DT::Presenter::Render::Surface::VideoPlaybackStyle style) {
    this->playbackStyle = style;
}

DT::Presenter::Render::Surface::VideoPlaybackStyle DT::Presenter::Render::Surface::VideoDecoder::GetPlaybackStyle() {
    return playbackStyle;
}

bool DT::Presenter::Render::Surface::VideoDecoder::HasNextFrame() {
    return this->frameDataBuffer != nullptr && this->frameDataBuffer->IsReady();
}

void DT::Presenter::Render::Surface::VideoDecoder::NextFrame(GLuint *rgb) {
    // move data to texture;
    GLenum imageFormat = GL_RGB;
    if (frameDataBuffer != nullptr) {
        if (*rgb <= 0) {
            glGenTextures(1, rgb);
            wxPrintf("RGBA Video texture id %d\n", *rgb);
            glCheckError();
            glBindTexture(GL_TEXTURE_2D, *rgb);
            glCheckError();
//            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glCheckError();

            glTexImage2D(
                    GL_TEXTURE_2D,
                    0,
                    imageFormat,
                    frameDataBuffer->GetWidth(),
                    frameDataBuffer->GetHeight(),
                    0,
                    imageFormat,
                    GL_UNSIGNED_BYTE,
                    frameDataBuffer->GetBuffer()
            );
            glCheckError();

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glCheckError();
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glCheckError();

            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glCheckError();
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glCheckError();
        } else {
            glBindTexture(GL_TEXTURE_2D, *rgb);
            glTexSubImage2D(
                    GL_TEXTURE_2D,
                    0,
                    0,
                    0,
                    frameDataBuffer->GetWidth(),
                    frameDataBuffer->GetHeight(),
                    imageFormat,
                    GL_UNSIGNED_BYTE,
                    frameDataBuffer->GetBuffer()
            );
        }
        frameDataBuffer->Reset();
    } else {
        if (*rgb > 0) {
            glBindTexture(GL_TEXTURE_2D, 0);
            glDeleteTextures(1, rgb);
            *rgb = 0;
        }
    }
}

void DT::Presenter::Render::Surface::VideoDecoder::Play() {
    av_seek_frame(this->formatContext, this->videoStreamIndex, 0, AVFMT_FLAG_FAST_SEEK);
    delete this->frameDataBuffer;
    this->frameDataBuffer = new BufferData();

    if (decoderThread == nullptr) {
        decoderThread = new DecoderThread(this);
        wxPrintf("A new decoder thread has been created\n");
    }
    if (!decoderThread->IsRunning()) {
        decoderThread->Run();
    }
    this->running = true;
}

void DT::Presenter::Render::Surface::VideoDecoder::Pause() {
    if (decoderThread) {
        decoderThread->Delete(nullptr, wxTHREAD_WAIT_DEFAULT);
        delete decoderThread;
        decoderThread = nullptr;
        wxPrintf("Decoder thread has been destroyed\n");
    }
    this->running = false;
    delete this->frameDataBuffer;
    this->frameDataBuffer = nullptr;
}

void DT::Presenter::Render::Surface::VideoDecoder::Stop() {
    if (decoderThread) {
        if (this->running) {
            decoderThread->Delete(nullptr, wxTHREAD_WAIT_DEFAULT);
            delete decoderThread;
            decoderThread = nullptr;
            wxPrintf("Decoder thread has been destroyed\n");
        }
    }
    this->running = false;
    delete this->frameDataBuffer;
    this->frameDataBuffer = nullptr;
}

bool DT::Presenter::Render::Surface::VideoDecoder::IsPlaying() {
    if (this->decoderThread != nullptr && !this->decoderThread->IsRunning()) {
        this->decoderThread->Delete(nullptr, wxTHREAD_WAIT_DEFAULT);
        delete this->decoderThread;
        this->decoderThread = nullptr;
    }
    return this->running && this->decoderThread != nullptr && this->decoderThread->IsRunning();
}
