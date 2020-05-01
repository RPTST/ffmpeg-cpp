#pragma once

#include "ffmpeg-cpp/ffmpeg.h"
#include <vector>

#ifdef __linux__
#include <string>
#endif

namespace ffmpegcpp {

	class OutputStream;

	class Muxer
	{
	public:

		Muxer(const char* fileName);
		~Muxer();

		void AddOutputStream(OutputStream* stream);

		void WritePacket(AVPacket* pkt);

		void Close();
		
		bool IsPrimed();

		AVCodec* GetDefaultVideoFormat();
		AVCodec* GetDefaultAudioFormat();


	private:

		void Open();
		
		std::vector<OutputStream*> outputStreams;
		std::vector<AVPacket*> packetQueue;

		AVOutputFormat* containerFormat;

		AVFormatContext* containerContext = nullptr;

		std::string m_fileName;

		void CleanUp();

		bool opened = false;
	};
}
