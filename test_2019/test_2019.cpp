#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <windows.h>
#include <Mmreg.h>
#include <mmeapi.h>
#include <Windows.h>
#include <math.h>

extern "C" {
//#include "fftw3.h"
#include "libavcodec/avcodec.h"
#include "libavutil/opt.h"
#include "libavformat/avformat.h"
#include "libswresample/swresample.h"
#include "libavutil/mathematics.h"
}

#pragma comment(lib, "C:/Program Files/ffmpeg/lib/avcodec.lib")
#pragma comment(lib, "C:/Program Files/ffmpeg/lib/avformat.lib")
#pragma comment(lib, "C:/Program Files/ffmpeg/lib/avutil.lib")
#pragma comment(lib, "C:/Program Files/ffmpeg/lib/avdevice.lib")
#pragma comment(lib, "C:/Program Files/ffmpeg/lib/swresample.lib")

#pragma comment(lib, "Winmm.lib")
//#pragma comment(lib, "C:/fftw/libfftw3-3.lib")
//#pragma comment(lib, "C:/fftw/libfftw3f-3.lib")
//#pragma comment(lib, "C:/fftw/libfftw3l-3.lib")

//#pragma comment(lib, "C:/opencv/build/install/x64/vc16/lib/opencv_world412d.lib")

#pragma warning(disable:4996)

#define RATE 44100
#define PIPE 2
#define PI 3.1415926
#define INBUF_SIZE 4096
#define AUDIO_INBUF_SIZE 20480
#define AUDIO_REFILL_THRESH 4096
#define AV_CODEC_MAX_AUDIO_FRAME_SIZE 16384
#define MAX_AUDIO_FRAME_SIZE 320000
#define SECOND 50

int decode_audio_file(HWAVEOUT hWavO, WAVEHDR whr, WAVEHDR whr2, const char* filename)
{
	av_register_all();
	avformat_network_init();
	
	AVFormatContext* format = avformat_alloc_context();
	if (avformat_open_input(&format, filename, NULL, NULL) != 0) {
		fprintf(stderr, "Could not open file '%s'\n", filename);
		return -1;
	}
	if (avformat_find_stream_info(format, NULL) < 0) {
		fprintf(stderr, "Could not retrieve stream info from file '%s'\n", filename);
		return -1;
	}

	av_dump_format(format, 0, filename, false);

	// Find the index of the first audio stream
	int stream_index = -1;
	for (int i = 0; i < format->nb_streams; i++)
	{
		if (format->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
			stream_index = i;
			break;
		}
	}
	if (stream_index == -1)
	{
		fprintf(stderr, "Could not retrieve audio stream from file '%s'\n", filename);
		return -1;
	}
	AVStream* stream = format->streams[stream_index];

	// find & open codec
	AVCodecContext* codec = stream->codec;
	if (avcodec_open2(codec, avcodec_find_decoder(codec->codec_id), NULL) < 0)
	{
		fprintf(stderr, "Failed to open decoder for stream #%u in file '%s'\n", stream_index, filename);
		return -1;
	}

	// prepare to read data
	AVPacket * packet = (AVPacket*)av_malloc(sizeof(AVPacket));
	av_init_packet(packet);

	AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
	
	int out_channels_layout = AV_CH_LAYOUT_STEREO;
	int out_channels = av_get_channel_layout_nb_channels(out_channels_layout);
	int out_buffer_size = av_samples_get_buffer_size(NULL, out_channels, codec->frame_size, out_sample_fmt, 1);
	PBYTE buffer = (uint8_t*)av_malloc(MAX_AUDIO_FRAME_SIZE * 2);

	AVFrame* frame = av_frame_alloc();
	if (!frame)
	{
		fprintf(stderr, "Error allocating the frame\n");
		return -1;
	}
	// prepare resampler
	struct SwrContext* swr = swr_alloc();

	av_opt_set_int(swr, "in_channel_count", codec->channels, 0);
	av_opt_set_int(swr, "out_channel_count", 2, 0);
	av_opt_set_int(swr, "in_channel_layout", av_get_default_channel_layout(codec->channels), 0);
	av_opt_set_int(swr, "out_channel_layout", out_channels_layout, 0);
	av_opt_set_int(swr, "in_sample_rate", codec->sample_rate, 0);
	av_opt_set_int(swr, "out_sample_rate", codec->sample_rate, 0);
	av_opt_set_sample_fmt(swr, "in_sample_fmt", codec->sample_fmt, 0);
	av_opt_set_sample_fmt(swr, "out_sample_fmt", out_sample_fmt, 0);
	swr_init(swr);
	if (!swr_is_initialized(swr)) {
		fprintf(stderr, "Resampler has not been properly initialized\n");
		return -1;
	}

	whr.lpData = (LPSTR)buffer;
	whr.dwBufferLength = out_buffer_size;

	whr2.lpData = (LPSTR)buffer;
	whr2.dwBufferLength = out_buffer_size;

	waveOutPrepareHeader(hWavO, &whr, sizeof(whr));

	waveOutPrepareHeader(hWavO, &whr2, sizeof(whr2));
	// iterate through frames
	while (av_read_frame(format, packet) >= 0) {
		if (packet->stream_index == stream_index)
		{
			int gotFrame;
			if (avcodec_decode_audio4(codec, frame, &gotFrame, packet) < 0) {
				break;
			}
			if (gotFrame > 0)
			{
				int frame_count = swr_convert(swr, &buffer,
					MAX_AUDIO_FRAME_SIZE,
					(const uint8_t**)frame->data,
					frame->nb_samples);

				
				for (int i = 0; i < codec->sample_rate * SECOND; i++)
				{
					waveOutWrite(hWavO, &whr, sizeof(whr));
				
					waveOutWrite(hWavO, &whr2, sizeof(whr2));
				}
			}
		}
		av_free_packet(packet);
	}
	
	// clean up
	av_free(buffer);
	av_frame_free(&frame);
	swr_free(&swr);
	avcodec_close(codec);
	avformat_free_context(format);

	return 0;
}

int audio_decode(/*const char * outfilename, */HWAVEOUT hWavO, WAVEHDR whr, WAVEHDR whr2, const char * filename)
{
	AVFormatContext* pFormatCtx;
	AVCodecContext* pCodecCtx;
	AVCodec* pCodec;
	AVPacket* packet;
	uint8_t* out_buffer;
	AVFrame* pFrame;
	int	audioStream;
	int got_picture;
	int index = 0;
	struct SwrContext* au_convert_ctx;

	//FILE* pFile = pFile = fopen(outfilename, "wb");

	av_register_all();
	avformat_network_init();
	pFormatCtx = avformat_alloc_context();

	if (avformat_open_input(&pFormatCtx, filename, NULL, NULL) != 0) {
		printf("Couldn't open input stream.\n");
		return -1;
	}
	// Retrieve stream information
	if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
		printf("Couldn't find stream information.\n");
		return -1;
	}
	// Dump valid information onto standard error
	av_dump_format(pFormatCtx, 0, filename, false);

	// Find the first audio stream
	audioStream = -1;
	for (int i = 0; i < pFormatCtx->nb_streams; i++)
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
			audioStream = i;
			break;
		}

	if (audioStream == -1) {
		printf("Didn't find a audio stream.\n");
		return -1;
	}

	// Get a pointer to the codec context for the audio stream
	pCodecCtx = pFormatCtx->streams[audioStream]->codec;

	// Find the decoder for the audio stream
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodec == NULL) {
		printf("Codec not found.\n");
		return -1;
	}

	// Open codec
	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
		printf("Could not open codec.\n");
		return -1;
	}

	packet = (AVPacket*)av_malloc(sizeof(AVPacket));
	av_init_packet(packet);

	//Out Audio Param
	uint64_t out_channel_layout = AV_CH_LAYOUT_STEREO;
	//nb_samples: AAC-1024 MP3-1152
	int out_nb_samples = pCodecCtx->frame_size;
	AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
	int out_sample_rate = pCodecCtx->sample_rate;
	int out_channels = av_get_channel_layout_nb_channels(out_channel_layout);
	//Out Buffer Size
	int out_buffer_size = av_samples_get_buffer_size(NULL, out_channels, out_nb_samples, out_sample_fmt, 1);

	out_buffer = (uint8_t*)av_malloc(MAX_AUDIO_FRAME_SIZE * 2);
	pFrame = av_frame_alloc();


	//FIX:Some Codec's Context Information is missing
	int64_t in_channel_layout = av_get_default_channel_layout(pCodecCtx->channels);
	//Swr

	au_convert_ctx = swr_alloc();
	au_convert_ctx = swr_alloc_set_opts(au_convert_ctx,
		out_channel_layout,
		out_sample_fmt,
		out_sample_rate,
		in_channel_layout,
		pCodecCtx->sample_fmt,
		pCodecCtx->sample_rate, 0, NULL);
	swr_init(au_convert_ctx);

	whr.lpData = (LPSTR)out_buffer;
	whr.dwBufferLength = out_buffer_size;
	waveOutPrepareHeader(hWavO, &whr, sizeof(whr));

	whr2.lpData = (LPSTR)out_buffer;
	whr2.dwBufferLength = out_buffer_size;
	waveOutPrepareHeader(hWavO, &whr2, sizeof(whr2));
	//waveOutSetPlaybackRate(hWavO, 0x8000);

	while (av_read_frame(pFormatCtx, packet) >= 0) {
		if (packet->stream_index == audioStream) {
			int ret = avcodec_decode_audio4(pCodecCtx, pFrame, &got_picture, packet);
			if (ret < 0) {
				printf("Error in decoding audio frame.\n");
				return -1;
			}
			if (got_picture > 0) {
				swr_convert(au_convert_ctx, &out_buffer,
					MAX_AUDIO_FRAME_SIZE, (const uint8_t**)pFrame->data,
					pFrame->nb_samples);
#if SHOW
				printf("index:%5d\t pts:%lld\t packet size:%d\n", index, packet->pts, packet->size);
#endif

				//fwrite(out_buffer, 1, out_buffer_size, pFile);
				
				for (int i = 0; i < pCodecCtx->sample_rate * SECOND; i++)
				{
					waveOutWrite(hWavO, &whr, sizeof(whr));
				}

				
				for (int i = 0; i < pCodecCtx->sample_rate * SECOND; i++)
				{
					waveOutWrite(hWavO, &whr2, sizeof(whr2));
				}
				
				index++;
			}

		}
		av_free_packet(packet);
	}

	swr_free(&au_convert_ctx);

	//fclose(pFile);

	av_free(out_buffer);
	avcodec_close(pCodecCtx);
	avformat_close_input(&pFormatCtx);
	return 0;
}

void GenerateAudio(HWAVEOUT hWavO, WAVEHDR whr)
{
	double freq[] = { 261.625, 293.664, 329.627, 349.228, 391.995, 440, 493.883, 523.251 };
	PBYTE buffer = new BYTE[RATE * SECOND];
	for (int j = 0; j < 8; j++)
	{
		double w = (2 * PI) / freq[j];
		for (int i = 0; i < RATE * SECOND; i++)
		{
			buffer[i] = 127 + 127 * std::sin(w * i);
		}
		
		whr.lpData = (LPSTR)buffer;
		whr.dwBufferLength = RATE * SECOND;
		waveOutPrepareHeader(hWavO, &whr, sizeof(whr));
		for (int i = 0; i < RATE; i++)
			waveOutWrite(hWavO, &whr, sizeof(whr));
	}
	delete[] buffer;
}

int main()
{
	//fftw_complex in[n], out[n];
	//fftw_plan p;

	//for (int i = 0; i < n; i++)
	//{
	//	in[i][0] = i;
	//	in[i][1] = 0;
	//}

	//p = fftw_plan_dft_1d(n, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
	//fftw_execute(p);
	//for (int i = 0; i < n; i++)
	//	std::cout << out[i][0] << "+" << out[i][1] << '\n';
	//fftw_destroy_plan(p);
	//fftw_cleanup();

	WAVEHDR whr, whr2;
	WAVEFORMATEX wfx;
	HWAVEOUT hWavO;

	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nSamplesPerSec = RATE;
	wfx.nChannels = PIPE;
	wfx.wBitsPerSample = 16;
	wfx.nBlockAlign = PIPE * wfx.wBitsPerSample / 8;
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
	wfx.cbSize = 0;


	if (waveOutOpen(&hWavO, WAVE_MAPPER, &wfx, NULL, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
		std::cout << "出错";

	whr.dwLoops = 1;
	whr.dwFlags = 0;
	whr2.dwLoops = 1;
	whr2.dwFlags = 0;

	
	//GenerateAudio(hWavO, whr);
	//audio_decode(hWavO, whr, whr2, "F:/Music/1563833285950.mp3");
	decode_audio_file(hWavO, whr, whr2, "F:/Music/中原めいこ - Cloudyな午后.mp3");

	waveOutUnprepareHeader(hWavO, &whr, sizeof(whr));
	waveOutUnprepareHeader(hWavO, &whr2, sizeof(whr2));
	waveOutClose(hWavO);
	return 0;
}
