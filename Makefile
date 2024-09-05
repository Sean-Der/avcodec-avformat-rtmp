all:
	rm -f out.h264 && g++ -std=c++17 main.cpp `pkg-config --libs --cflags libavcodec libavformat libavutil` && ./a.out
