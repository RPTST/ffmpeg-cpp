g++ -I..  -Wall -std=c++11  \
          -o read_audio decode_audio.cpp \
          -L../../../build/ -lffmpeg-cpp \
          `pkg-config --cflags --libs libavutil libavcodec libswscale libavformat`
