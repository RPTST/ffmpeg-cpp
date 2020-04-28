
#include "RawVideoFileSource.h"
#include "FFmpegException.h"
#include <iostream>

using namespace std;

namespace ffmpegcpp
{
    RawVideoFileSource::RawVideoFileSource(const char* fileName, FrameSink* frameSink)
    {
#ifdef DEBUG
        std::cerr << "Currently in " <<  __func__ << ". We are opening (fileName)  :  "  <<  fileName << "\n";
#endif
        // create the demuxer - it can handle figuring out the video type on its own apparently
        try
        {
#ifdef DEBUG
            std::cerr << "Trying to create new Demuxer ... " << "\n";
#endif
            demuxer = new Demuxer(fileName, NULL, NULL);
#ifdef DEBUG
            std::cerr << "New Demuxer created " << "\n";
#endif
            demuxer->DecodeBestVideoStream(frameSink);

#ifdef DEBUG
            std::cerr << "demuxer->DecodeBestVideoStream(frameSink) done " << "\n";
#endif

        }
        catch (FFmpegException e)
        {
            CleanUp();
            throw e;
        }
    }
    // only testng on Linux for the moment, but Windows should work. MacOS X = don't care
#ifdef __linux__

#ifndef DEBUG
#define DEBUG
#endif
    /*
        Demuxer video4linux2,v4l2 [Video4Linux2 device grab]:
        V4L2 indev AVOptions:
          -standard          <string>     .D....... set TV standard, used only by analog frame grabber
          -channel           <int>        .D....... set TV channel, used only by frame grabber (from -1 to INT_MAX) (default -1)
          -video_size        <image_size> .D....... set frame size
          -pixel_format      <string>     .D....... set preferred pixel format
          -input_format      <string>     .D....... set preferred pixel format (for raw video) or codec name
          -framerate         <string>     .D....... set frame rate
          -list_formats      <int>        .D....... list available formats and exit (from 0 to INT_MAX) (default 0)
             all                          .D....... show all available formats
             raw                          .D....... show only non-compressed formats
             compressed                   .D....... show only compressed formats
          -list_standards    <int>        .D....... list supported standards and exit (from 0 to 1) (default 0)
             all                          .D....... show all supported standards
          -timestamps        <int>        .D....... set type of timestamps for grabbed frames (from 0 to 2) (default default)
             default                      .D....... use timestamps from the kernel
             abs                          .D....... use absolute timestamps (wall clock)
             mono2abs                     .D....... force conversion from monotonic to absolute timestamps
          -ts                <int>        .D....... set type of timestamps for grabbed frames (from 0 to 2) (default default)
             default                      .D....... use timestamps from the kernel
             abs                          .D....... use absolute timestamps (wall clock)
             mono2abs                     .D....... force conversion from monotonic to absolute timestamps
          -use_libv4l2       <boolean>    .D....... use libv4l2 (v4l-utils) conversion functions (default false)
    */

    RawVideoFileSource::RawVideoFileSource(const char* fileName, int d_width, int d_height, int d_framerate, AVPixelFormat format, FrameSink * /*aframeSink*/)
    {
        // mandatory under Linux
        avdevice_register_all();

        width  = d_width;
        height = d_height;
        framerate = d_framerate;
#ifdef _WIN32
        // Fixed by the operating system
        const char * input_device = "dshow"; // I'm using dshow when cross compiling :-)
#elif defined(__linux__)
        // libavutil, pixdesc.h
        const char * pix_fmt_name = av_get_pix_fmt_name(format);
        const char * pix_fmt_name2 = av_get_pix_fmt_name(AV_PIX_FMT_YUVJ420P); // = "mjpeg"
        enum AVPixelFormat pix_name = av_get_pix_fmt("mjpeg");
        enum AVPixelFormat pix_name2 = av_get_pix_fmt("rawvideo");

        cout<<"AVPixelFormat name of AV_PIX_FMT_YUV420P : " << pix_fmt_name << "\n";
        cout<<"AVPixelFormat name of AV_PIX_FMT_YUVJ420P : " << pix_fmt_name2 << "\n";
        cout<<"PixelFormat value for \"mjpeg\" : " << pix_name << "\n";
        cout<<"PixelFormat value for \"rawvideo\" : " << pix_name2 << "\n";

        // Fixed by the operating system
        const char * input_device = "v4l2";
        const char * device_name = "/dev/video0";
#endif

#ifdef DEBUG
        std::cerr << "Currently in "       <<  __func__    << ". We are opening (fileName)  :  "  <<  fileName << std::endl;
        std::cerr << "width "              << width        << "\n";
        std::cerr << "height "             << height       << "\n";
        std::cerr << "format "             << format       << "\n";
        std::cerr << "filename "           << fileName     << "\n";
        std::cerr << "device_name "        << device_name  << "\n";
        std::cerr << "framerate (int) :  " << framerate    << "\n";
#endif
        //  /!\ v4l2  is a DEMUXER for ffmpeg !!!  (not a device or format or whatever else !! )
        // important: AVCodecContext can be freed on failure (easy with mjpeg ...)
        //int VideoStreamIndx = -1;

        ///pAVCodecContext = NULL;
        pAVCodec = NULL;

        pAVFormatContextIn = NULL;
        options = NULL;

        pAVFormatContextIn = avformat_alloc_context();
        pAVFormatContextIn->video_codec_id = AV_CODEC_ID_MJPEG;

        inputFormat = av_find_input_format(input_device);

        // WORKS OK TOO
        char videoSize[32];
        sprintf(videoSize, "%dx%d", width, height);
        av_dict_set(&options, "video_size", videoSize, 0);
        // Other (fixed) way :
        // av_dict_set(&options, "video_size", "1280x720", 0);
        //av_dict_set(&options, "video_size", "1920x1080", 0);

        const char * framerate_option_name = "frameRate";
        char frameRateValue[10];
        sprintf(frameRateValue, "%d", framerate);

#ifdef DEBUG
        std::cerr << "framerate_option_name :  " << framerate_option_name  << "\n";
        std::cerr << "frameRateValue        :  " << frameRateValue  << "\n";
#endif
        av_dict_set(&options, "framerate", "30", 0);
//        av_dict_set(&options, framerate_option_name, frameRateValue, 0);
        av_dict_set(&options, "pixel_format", pix_fmt_name2, 0);  //  "mjpeg" "yuvj420p"
        //av_dict_set(&options, "pixel_format", pix_fmt_name2, 0);  //  "mjpeg" "yuvj420p"

        ///TEST
        //av_dict_set(&options, "pix_fmt", "yuvj420p", 0);  //  "mjpeg" "yuvj420p"
        av_dict_set(&options, "use_wallclock_as_timestamps", "1", 0);

        try
        {
#ifdef DEBUG
            std::cerr << "Creating new demuxer "   << "\n";
#endif
            demuxer = new Demuxer(fileName, inputFormat, pAVFormatContextIn, options);
#ifdef DEBUG
            std::cout << "demuxer created "   << "\n";
#endif
//            demuxer->DecodeVideoStream(VideoStreamIndx, aframeSink);
#ifdef DEBUG
//            std::cout << "demuxer->DecodeBestVideoStream(frameSink) DONE "   << "\n";
#endif

        }
        catch (FFmpegException e)
        {
            CleanUp();
            throw e;
        }

/*
        if ((avformat_open_input(&pAVFormatContextIn, device_name, inputFormat, &options))>=0)
        {
            // TODO : verify it's mandatory
#ifdef DEBUG
            std::cerr <<  ">>>  av_input_open_returned 1 " << "\n";
#endif

            // create the demuxer
            try
            {
#ifdef DEBUG
                std::cerr << "Creating new demuxer "   << "\n";
#endif
                // inspired from https://code.mythtv.org/trac/ticket/13186?cversion=0&cnum_hist=2
                pAVCodecContext = avcodec_alloc_context3(pAVCodec);
                pAVCodec = avcodec_find_decoder(pAVFormatContextIn->streams[VideoStreamIndx]->codecpar->codec_id);
                avcodec_parameters_to_context(pAVCodecContext, pAVFormatContextIn->streams[VideoStreamIndx]->codecpar);

                if(pAVCodec == NULL)
                    fprintf(stderr,"Unsupported codec !");

                int value = avcodec_open2(pAVCodecContext , pAVCodec , NULL);

                if( value < 0)
                    cout<<"Error : Could not open codec";

                // DONE : everything works already separately : 
                //   - pass the parameters (device name, width, height, framerate and pixel format)
                //   - initialise the AVContext,
                //   - initialize the AVCodec, and fill it with correct values
                //   - find the video stream, and fill it withcorrect values
                //   - initialize the device (/dev/video0 here) with avformat_open_input() 
                //     => the light is on, right video_size, framerate, colors, and so on
                //  
                // FIXME BROKEN : next step : create the demuxer is broken

                /// maybe simply open a file, and grab frames instead ?

                demuxer = new Demuxer(fileName, inputFormat, options);
#ifdef DEBUG
                std::cout << "Muxer created "   << "\n";
#endif
                demuxer->DecodeVideoStream(VideoStreamIndx, aframeSink);
#ifdef DEBUG
                std::cout << "demuxer->DecodeBestVideoStream(frameSink) DONE "   << "\n";
#endif
            }
            catch (FFmpegException e)
            {
                CleanUp();
                throw e;
            }
        }
*/

#ifdef DEBUG
            std::cerr << "returning from " << __func__  << "\n";
#endif
        }
#endif  /*  __linux__ */

        // Doesn't work for now. See the header for more info.
        /*RawVideoFileSource::RawVideoFileSource(const char* fileName, int width, int height, const char* frameRate, AVPixelFormat format, VideoFrameSink* frameSink)
        {
            // try to deduce the input format from the input format name
            AVInputFormat *file_iformat;
            if (!(file_iformat = av_find_input_format("yuv4mpegpipe")))
            {
                CleanUp();
                throw FFmpegException("Unknown input format 'rawvideo'");
            }

            AVDictionary* format_opts = NULL;

            // only set the frame rate if the format allows it!
            if (file_iformat && file_iformat->priv_class &&	av_opt_find(&file_iformat->priv_class, "framerate", NULL, 0, AV_OPT_SEARCH_FAKE_OBJ))
            {
                av_dict_set(&format_opts, "framerate", frameRate, 0);
            }
            char videoSize[200];
            sprintf(videoSize, "%dx%d", width, height);
            av_dict_set(&format_opts, "video_size", videoSize, 0);
            const char* pixelFormatName = av_get_pix_fmt_name(format);
            av_dict_set(&format_opts, "pixel_format", pixelFormatName, 0);

            // create the demuxer
            try
            {
                demuxer = new Demuxer(fileName, file_iformat, format_opts);
                demuxer->DecodeBestVideoStream(frameSink);
            }
            catch (FFmpegException e)
            {
                CleanUp();
                throw e;
            }
        }*/

    RawVideoFileSource::~RawVideoFileSource()
    {
        CleanUp();
    }

    void RawVideoFileSource::CleanUp()
    {
        if (demuxer != nullptr)
        {
            avformat_close_input(&pAVFormatContextIn);
            avformat_free_context(pAVFormatContextIn);
            av_dict_free(&options);

            delete demuxer;
            demuxer = nullptr;
        }
    }

    void RawVideoFileSource::PreparePipeline()
    {
        demuxer->PreparePipeline();
    }

    bool RawVideoFileSource::IsDone()
    {
        return demuxer->IsDone();
    }

    void RawVideoFileSource::Step()
    {
        demuxer->Step();
    }
}

