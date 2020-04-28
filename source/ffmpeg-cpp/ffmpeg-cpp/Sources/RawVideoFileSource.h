#pragma once

#include "ffmpeg-cpp/ffmpeg.h"

#include "InputSource.h"
#include "Demuxer.h"


namespace ffmpegcpp
{
	class RawVideoFileSource : public InputSource
	{
	public:

		RawVideoFileSource(const char* fileName, FrameSink* frameSink);

                RawVideoFileSource(const char*, int, int, int, AVPixelFormat, FrameSink* frameSink);
		// I couldn't get this to work. The thing is that it also crashes weirdly when I run ffmpeg directly,
		// so I think it's more an issue of ffmpeg than one of my library.
		//RawVideoFileSource(const char* fileName, int width, int height, const char* frameRate, AVPixelFormat format, VideoFrameSink* frameSink);
		virtual ~RawVideoFileSource();

		virtual void PreparePipeline();
		virtual bool IsDone();
		virtual void Step();

	private:
		AVFormatContext    * pAVFormatContextIn;
		AVDictionary       * options;
		enum AVPixelFormat   format;
		AVCodec            * pAVCodec;
		AVInputFormat      * inputFormat;
		int                  width;
		int                  height;
		int                  framerate;
		void                 CleanUp();
		Demuxer            * demuxer;
//		FrameSink          * frameSink;
	};
}
