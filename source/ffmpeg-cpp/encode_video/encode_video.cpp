#include <iostream>
#include "ffmpegcpp.h"

#define MJPEG_VIDEO
//#define MPEG4_VIDEO
//#define MPEG2_VIDEO
//#define H264_VIDEO

using std::cerr;
using std::cout;

using namespace ffmpegcpp;


using namespace std;
using namespace ffmpegcpp;

//TODO : use existing interface instead ?

class PGMFileSink : public VideoFrameSink, public FrameWriter
{
public:

    PGMFileSink()
    {
    }

    FrameSinkStream* CreateStream()
    {
        stream = new FrameSinkStream(this, 0);
        return stream;
    }

    virtual void WriteFrame(int /* streamIndex */, AVFrame* frame, StreamData*  /* streamData */)
    {
        ++frameNumber;
        printf("saving frame %3d\n", frameNumber);
        fflush(stdout);

        // write the first channel's color data to a PGM file.
        // This raw image file can be opened with most image editing programs.
        snprintf(fileNameBuffer, sizeof(fileNameBuffer), "frames/frame-%d.pgm", frameNumber);
        pgm_save(frame->data[0], frame->linesize[0], frame->width, frame->height, fileNameBuffer);
    }

    void pgm_save(unsigned char *bufy, int wrap, int xsize, int ysize, char *filename)
    {
        FILE *f;
        int i;
        f = fopen(filename, "w");

        fprintf(f, "P5\n%d %d\n%d\n", xsize, ysize, 255);

        for (i = 0; i < ysize; i++)
        {
            fwrite(bufy + i * wrap, 1, xsize, f);
        }
        fclose(f);
    }

    virtual void Close(int /* streamIndex */)
    {
        delete stream;
    }

    virtual bool IsPrimed()
    {
        // Return whether we have all information we need to start writing out data.
        // Since we don't really need any data in this use case, we are always ready.
        // A container might only be primed once it received at least one frame from each source
        // it will be muxing together (see Muxer.cpp for how this would work then).
        return true;
    }

private:
    char fileNameBuffer[1024];
    int frameNumber = 0;
    FrameSinkStream* stream;

};



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

        std::cerr << "Using MJPEG input Codec" << "\n";

        // Create a MPEG2 codec that will encode the raw data.
        // VideoCodec * codec = new VideoCodec(AV_CODEC_ID_MJPEG);
        //TEST
        MJPEGCodec * codec = new MJPEGCodec();

        //  OUTPUT CODEC, linked to the encoder ...
        H264Codec  * vcodec = new H264Codec();
        vcodec->SetGenericOption("b", "2M");
        vcodec->SetGenericOption("bit_rate", "2M");
        vcodec->SetProfile("high10"); // baseline, main, high, high10, high422
        vcodec->SetTune("film");  // film animation grain stillimage psnr ssim fastdecode zerolatency
        vcodec->SetPreset("veryslow"); // fast, medium, slow slower, veryslow placebo
        vcodec->SetCrf(23);

        // TODO : check that ...
        // codec->SetGenericOption("bitrate", "2M");
        // codec->SetGenericOption("timestamp_time", "now");

        int width = 1280;
        int height = 720;
        AVRational frameRate = {1, 30};
        AVPixelFormat pix_format = AV_PIX_FMT_YUVJ420P; // = V4L2_PIX_FMT_MJPEG

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

        // normal code. Kept for the record
#ifdef MJPEG_VIDEO
        PGMFileSink* fileSink = new PGMFileSink();

        RawVideoFileSource* videoFile = new RawVideoFileSource("/dev/video0", 1280, 720, frameRate.den, pix_format);
        videoFile->setFrameSink(fileSink);;
        videoFile->demuxer->DecodeBestVideoStream(fileSink);
#else
        // Create an encoder that will encode the raw audio data as MP3. Tie it to the muxer so it will be written to the file.
        VideoEncoder* encoder = new VideoEncoder(codec, muxer);

        //RawVideoFileSource* videoFile = new RawVideoFileSource(videoContainer->GetFileName(), encoder);
        RawVideoFileSource* videoFile = new RawVideoFileSource("../samples/big_buck_bunny.mp4", encoder);
#endif
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
