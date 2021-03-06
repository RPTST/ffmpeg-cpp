
#include "ffmpeg-cpp/ffmpeg.h"

#include "Muxing/Muxer.h"

#include "Sources/Demuxer.h"
#include "Sources/InputSource.h"
#include "Sources/RawAudioFileSource.h"
#include "Sources/RawVideoFileSource.h"
#include "Sources/RawAudioDataSource.h"
#include "Sources/RawVideoDataSource.h"
#include "Sources/EncodedFileSource.h"

#include "ffmpeg-cpp/Frame Sinks/VideoEncoder.h"
#include "ffmpeg-cpp/Frame Sinks/AudioEncoder.h"
#include "ffmpeg-cpp/Frame Sinks/Filter.h"

#include "ffmpeg-cpp/Codecs/AudioCodec.h"

#include "ffmpeg-cpp/codecs/H265NVEncCodec.h"
#include "ffmpeg-cpp/codecs/H264NVEncCodec.h"
#include "ffmpeg-cpp/Codecs/VP9Codec.h"
#include "ffmpeg-cpp/Codecs/PNGCodec.h"
#include "ffmpeg-cpp/Codecs/JPGCodec.h"
#include "ffmpeg-cpp/Codecs/MJPEGCodec.h"
#include "ffmpeg-cpp/Codecs/H264Codec.h"
#include "ffmpeg-cpp/Codecs/MPEG4Codec.h"
#include "ffmpeg-cpp/Codecs/VideoCodec.h"
#include "ffmpeg-cpp/Codecs/Codec.h"

#include "FFmpegException.h"

#ifdef __linux__
#define AV_PIX_FMT_VAAPI_VLD AV_PIX_FMT_VAAPI
#endif
