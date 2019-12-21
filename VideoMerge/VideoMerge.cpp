extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}
#include <iostream>
#include <cstdlib>
using namespace std;

// (1) exe l input                                list the last key frame
// (2) exe m input1 input2 keyframeIndex output   mix the given two streams
// only design for closed-gop avc/hevc
int main(int args, char** argv) {
	if (args >= 3 && args <= 6) {
		if (argv[1][0] == 'l') {
			auto in = argv[2];

			// open the input file
			AVFormatContext* inFmtCtx = nullptr;
			avformat_open_input(&inFmtCtx, in, nullptr, nullptr);
			avformat_find_stream_info(inFmtCtx, nullptr);
			// find the video stream
			int videoStream = av_find_best_stream(inFmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
			AVStream* stream = inFmtCtx->streams[videoStream];
			av_dump_format(inFmtCtx, 0, in, 0);

			AVPacket packet;
			int frameCount = 0;
			int keyframe = 0;
			while (true) {
				// read one frame
				auto ret = av_read_frame(inFmtCtx, &packet);
				if (ret >= 0) {
					// select the video packet
					if (packet.stream_index == videoStream) {
						// record the last keyframe
						if ((packet.flags & AV_PKT_FLAG_KEY) > 0) {
							keyframe = frameCount;
						}
						frameCount++;
					}
				}
				else
					break;
			}
			std::cout << "\nThe last keyframe is: " << keyframe << '\n';
			avformat_close_input(&inFmtCtx);
		}
		else if (argv[1][0] == 'm') {
			auto in = argv[2];
			auto keyIndex = std::atoi(argv[4]);
			auto out = argv[5];

			// open the first input file
			AVFormatContext* inFmtCtx = nullptr;
			avformat_open_input(&inFmtCtx, in, nullptr, nullptr);
			avformat_find_stream_info(inFmtCtx, nullptr);
			int videoStream = av_find_best_stream(inFmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
			AVStream* inStream = inFmtCtx->streams[videoStream];

			// open the output file
			AVFormatContext* outFmtCtx = avformat_alloc_context();
			outFmtCtx->oformat = av_guess_format(nullptr, out, nullptr);
			avio_open(&outFmtCtx->pb, out, AVIO_FLAG_WRITE);

			AVStream* outStream = avformat_new_stream(outFmtCtx, nullptr);
			avcodec_parameters_copy(outStream->codecpar, inStream->codecpar);
			outStream->time_base = inStream->time_base;

			avformat_write_header(outFmtCtx, nullptr);

			AVPacket packet;
			int frameCount = 0;
			// save the timestamp of last keyframe for the second stream
			int64_t pts;

			// write the first stream
			while (true) {
				auto ret = av_read_frame(inFmtCtx, &packet);
				if (ret >= 0 && frameCount < keyIndex) {
					if (packet.stream_index == videoStream) {
						ret = av_interleaved_write_frame(outFmtCtx, &packet);
						frameCount++;
					}
				}
				else {
					if ((packet.flags & AV_PKT_FLAG_KEY) == 0) {
						std::cout << "The frame at the selected index isn't a keyframe.\n";
						exit(-1);
					}
					else {
						pts = packet.pts;
						break;
					}
				}
			}
			avformat_close_input(&inFmtCtx);

			// open the second input file and write the stream
			in = argv[3];
			inFmtCtx = nullptr;
			avformat_open_input(&inFmtCtx, in, nullptr, nullptr);
			avformat_find_stream_info(inFmtCtx, nullptr);
			videoStream = av_find_best_stream(inFmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
			inStream = inFmtCtx->streams[videoStream];

			// write the second stream
			while (true) {
				auto ret = av_read_frame(inFmtCtx, &packet);
				if (ret >= 0) {
					if (packet.stream_index == videoStream) {
						// don't know if it always works
						packet.pts += pts;
						packet.dts += pts;
						ret = av_interleaved_write_frame(outFmtCtx, &packet);
					}
				}
				else
					break;
			}

			av_write_trailer(outFmtCtx);
			avformat_close_input(&inFmtCtx);

			avio_context_free(&outFmtCtx->pb);
			avformat_free_context(outFmtCtx);

			std::cout << "Done!\n";
		}
	}
}