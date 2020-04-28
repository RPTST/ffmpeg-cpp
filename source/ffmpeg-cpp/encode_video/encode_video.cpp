#include <iostream>
#include "ffmpegcpp.h"

//#define MJPEG_VIDEO
//#define MPEG4_VIDEO
//#define MPEG2_VIDEO
#define H264_VIDEO

using std::cerr;
using std::cout;

using namespace ffmpegcpp;

int main()
{
    avdevice_register_all();

    // This example will take a raw audio file and encode it into as MP3.
    try
    {
        // Create a muxer that will output the video as MKV.
        Muxer* muxer = new Muxer("output.mpg");
#ifdef MPEG2_VIDEO
        std::cerr << "Using MPEG2 Codec" << "\n";

        // Create a MPEG2 codec that will encode the raw data.
        VideoCodec * codec = new VideoCodec("mpeg2video");
        //VideoCodec * codec = new VideoCodec(AV_CODEC_ID_MJPEG);
        // Set the global quality of the video encoding. This maps to the command line
        // parameter -qscale and must be within range [0,31].
        codec->SetQualityScale(23);
        // Set the bit rate option -b:v 2M
        codec->SetGenericOption("b", "2M");
#endif

#ifdef MJPEG_VIDEO

        std::cerr << "Using MJPEG Codec" << "\n";

        // Create a MPEG2 codec that will encode the raw data.
        // VideoCodec * codec = new VideoCodec(AV_CODEC_ID_MJPEG);
        //TEST
        MJPEGCodec * codec = new MJPEGCodec();

        // TODO : check that ...
        // codec->SetGenericOption("bitrate", "2M");
        // codec->SetGenericOption("timestamp_time", "now");

        int width = 1280;
        int height = 720;
        AVRational frameRate = {1, 30};
        AVPixelFormat pix_format = AV_PIX_FMT_YUVJ420P; // = V4L2_PIX_FMT_MJPEG

        // -t duration         record or transcode "duration" seconds of audio/video
        // -to time_stop       record or transcode stop time
        // -fs limit_size      set the limit file size in bytes
        // -ss time_off        set the start time offset
        // -sseof time_off     set the start time offset relative to EOF
        // -seek_timestamp     enable/disable seeking by timestamp with -ss
        // -timestamp time     set the recording timestamp ('now' to set the current time)

        // ~ $ ffmpeg -h encoder=mjpeg
        // Encoder mjpeg [MJPEG (Motion JPEG)]:
        // General capabilities: threads intraonly. Threading capabilities: frame and slice. Supported pixel formats: yuvj420p yuvj422p yuvj444p
        //
        // (lot of lines ... )
        // ffmpeg -h format=yuvj420p
 
        // TRACKS :   ffmpeg -f v4l2 -list_formats all -i /dev/video0
        // returns : [video4linux2,v4l2 @ 0x55e911c86780] 
        //Raw       :     yuyv422 :           
        // YUYV 4:2:2 : 640x480 160x90 160x120 176x144 320x180 320x240 352x288 432x240 640x360 800x448 800x600 864x480 960x720 1024x576
        //              1280x720 1600x896 1920x1080 2304x1296 2304x1536
        // [video4linux2,v4l2 @ 0x55e911c86780] Compressed:       mjpeg :        
        // Motion-JPEG : 640x480 160x90 160x120 176x144 320x180 320x240 352x288 432x240 640x360 800x448 800x600 864x480 960x720 1024x576
        // 1280x720 1600x896 1920x1080  /dev/video0: Immediate exit requested

        // + found in /usr/include/linux/videodev2.h:
        //#define V4L2_PIX_FMT_MJPEG    v4l2_fourcc('M', 'J', 'P', 'G') /* Motion-JPEG   */
        ///AVPixelFormat pix_format = V4L2_PIX_FMT_MJPEG;

        codec->Open(width, height, &frameRate, pix_format);
        std::cerr << "codec Open() done" << "\n";

        // Set the global quality of the video encoding. This maps to the command line
        // parameter -qscale and must be within range [0,31].
        //        codec->SetQualityScale(23);
#endif

#ifdef H264_VIDEO
        std::cerr << "Using H264 Codec" << "\n";

        // Default is : 
        // profile = main, crf = 10, preset = medium (default), tune = film or animation
        H264Codec* codec = new H264Codec();
        // TODO / FIXME
        // H264_VAAPICodec* codec = new H264_VAAPICodec();
        // FIXME : the buffer size seems to NOT being set ?
        codec->SetGenericOption("b", "2M");
        codec->SetGenericOption("bit_rate", "2M");
        //codec->SetGenericOption("low_power", true);
        codec->SetProfile("high10"); // baseline, main, high, high10, high422
        codec->SetTune("film");  // film animation grain stillimage psnr ssim fastdecode zerolatency
        codec->SetPreset("veryslow"); // fast, medium, slow slower, veryslow placebo

        // https://slhck.info/video/2017/03/01/rate-control.html
        //https://slhck.info/video/2017/02/24/crf-guide.html
        // about crf (very important !! )
        //
        //   0       <------   18  <-------  23 -----> 28 ------> 51
        //  lossless       better                 worse          worst
        codec->SetCrf(23);
#endif

#ifdef MPEG4_VIDEO
        std::cerr << "Using MPEG4 Codec" << "\n";

        MPEG4Codec* codec = new MPEG4Codec();

        // Default is : 
        // profile = main, crf = 10, preset = medium (default), tune = film or animation
        // FIXME : the buffer size seems to NOT being set ?
        codec->SetGenericOption("b", "4M");
        codec->SetGenericOption("bit_rate", "2M");
        codec->SetProfile("high10"); // baseline, main, high, high10, high422
        codec->SetTune("film");  // film animation grain stillimage psnr ssim fastdecode zerolatency
        codec->SetPreset("veryslow"); // fast, medium, slow slower, veryslow placebo
        codec->SetCrf(23);
#endif
                                    // GENERIC CODE

        // Load the raw video file so we can process it. FFmpeg is very good at deducing the file format, 
        // even from raw video files, but if we have something weird, we can specify the properties of the format in the constructor as commented out below.

        std::cerr << "RawVideoFileSource creation ..." << "\n";

        /*
            gdb) set codec->codecContext->bit_rate=400000
            (gdb) set encoder->finalFrameRate={1,30}
            (gdb) set encoder->finalPixelFormat=AV_PIX_FMT_YUVJ420P
            (gdb) set encoder->codec=codec
        */

        // normal code. Kept for the record
#ifdef MJPEG_VIDEO
        // Create an encoder that will encode the raw video data as mpg. Tie it to the muxer so it will be written to the file.
        // VideoEncoder* encoder = new VideoEncoder(codec, muxer, frameRate, pix_format);
        VideoEncoder* encoder = new VideoEncoder(codec, muxer);

        //RawVideoFileSource* videoFile = new RawVideoFileSource("/dev/video0", 1280, 720, pix_format, encoder);
        RawVideoFileSource* videoFile = new RawVideoFileSource("/dev/video0", 1280, 720, frameRate.den, pix_format, encoder);
#else
        // Create an encoder that will encode the raw audio data as MP3. Tie it to the muxer so it will be written to the file.
        VideoEncoder* encoder = new VideoEncoder(codec, muxer);

        //RawVideoFileSource* videoFile = new RawVideoFileSource(videoContainer->GetFileName(), encoder);
        RawVideoFileSource* videoFile = new RawVideoFileSource("../samples/big_buck_bunny.mp4", encoder);
#endif
        std::cerr << "RawVideoFileSource creation ... done" << "\n";
        std::cerr << "Entering in : videoFile->PreparePipeline()" << "\n";

        // Prepare the output pipeline. This will push a small amount of frames to the file sink until it IsPrimed returns true.
        videoFile->PreparePipeline();
        std::cerr << "videoFile->PreparePipeline() done ..." << "\n";

        // Push all the remaining frames through.
        while (!videoFile->IsDone())
        {

#ifdef MJPEG_VIDEO
            std::cerr <<  "in the loop ..." << "\n";
#endif
            videoFile->Step();
        }
        // Save everything to disk by closing the muxer.
        muxer->Close();
    }

    catch (FFmpegException e)
    {
        cerr << "Exception caught!" << "\n";
        cerr << e.what() << "\n";
        throw e;
    }
    cout << "Encoding complete!" << "\n";
}
