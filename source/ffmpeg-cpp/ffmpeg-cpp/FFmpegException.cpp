#include "FFmpegException.h"

using namespace std;

namespace ffmpegcpp
{
	FFmpegException::FFmpegException(const char * error) : exception ()
	{
	}

	FFmpegException::FFmpegException(const char * error, int returnValue)
		: exception()
	{
			std::string(std::string(error) + ": " + av_make_error_string(this->error, AV_ERROR_MAX_STRING_SIZE, returnValue)).c_str(), returnValue;
	}
}

/*
namespace ffmpegcpp
{
	FFmpegException::FFmpegException(const char * error) : exception ()
	{
	}

	FFmpegException::FFmpegException(const char * error, int returnValue)
		: exception(
			std::string(std::string(error) + ": " + av_make_error_string(this->error, AV_ERROR_MAX_STRING_SIZE, returnValue)).c_str(), returnValue )
	{
	}
}
*/