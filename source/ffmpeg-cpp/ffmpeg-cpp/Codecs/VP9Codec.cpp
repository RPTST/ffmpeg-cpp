#include "VP9Codec.h"

namespace ffmpegcpp
{

	VP9Codec::VP9Codec()
#ifdef __linux__
		: VideoCodec(AV_CODEC_ID_VP9)
//		: VideoCodec("libvpx.so")
#else
		: VideoCodec("libvpx-vp9")
#endif
	{

	}

	void VP9Codec::SetDeadline(const char* deadline)
	{
		SetOption("deadline", deadline);
	}

	void VP9Codec::SetCpuUsed(int cpuUsed)
	{
		SetOption("cpu-used", cpuUsed);
	}

	void VP9Codec::SetLossless(bool lossless)
	{
		SetOption("lossless", (lossless ? 1 : 0));
	}

	void VP9Codec::SetCrf(int crf)
	{
		SetOption("crf", crf);
	}


}