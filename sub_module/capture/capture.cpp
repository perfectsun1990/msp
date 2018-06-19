
#ifdef __cplusplus
extern "C"
{
#endif
#include <SDL.h>
#include <SDL_thread.h>

#include "libavutil/avstring.h"
#include "libavutil/imgutils.h"
#include "libavutil/opt.h"
#include "libavutil/parseutils.h"
#include "libavutil/pixdesc.h"
#include "libavutil/pixfmt.h"
#include "libavutil/time.h"
#include "libavutil/mem.h"
#include "libavutil/timestamp.h"

#include "libavdevice/avdevice.h"
#include "libavfilter/avfilter.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswresample/swresample.h"
#include "libswscale/swscale.h"

#undef main 
#ifdef __cplusplus
}
#endif

#include <iostream>
#include "capture.hpp"
#include "renderimp.hpp"
#include "dshow.hpp"
#include "pubcore.hpp"

#ifdef WIN32
#include <windows.h>
#include <corecrt_io.h>
#else
#include <unistd.h>
#endif
#include <chrono>



using second_t = std::chrono::duration<int32_t>;
using millis_t = std::chrono::duration<int32_t, std::milli>;
using micros_t = std::chrono::duration<int32_t, std::micro>;
using nannos_t = std::chrono::duration<int32_t, std::nano >;


#include <iostream>
#include <memory>
#include <thread>
#include <future>
#include <atomic>
#include <chrono>
#include <vector>
#include <string>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <functional>
#include <cassert>

#include <list>
#include <tuple>

class MpTimer
{
public:
	MpTimer() : m_timer_stopd(true), m_try_to_stop(false) {}
	~MpTimer() { stopd(); }

	void start(millis_t after, std::function<void()> task, bool loop = true)
	{
		{// 添加定时任务.
			std::lock_guard<std::mutex> locker(m_timer_mutex);
			m_timer_elist.push_back(std::tuple<bool, std::chrono::high_resolution_clock::time_point, millis_t, std::function<void()>>
				(loop, std::chrono::high_resolution_clock::now() + after, after, task));
		}

		if (m_timer_stopd == false)
			return;

		m_timing_time_point = std::chrono::high_resolution_clock::now();
		m_timer_stopd = false;

		std::thread([this, after, task]()
		{
			for (int64_t i = 0; !m_try_to_stop; ++i)
			{
				{//执行定时任务，检测剔除.
					std::lock_guard<std::mutex> locker(m_timer_mutex);
					if (!m_timer_elist.empty())
					{
						for (auto item = m_timer_elist.begin(); item != m_timer_elist.end(); item++)
							if (std::chrono::high_resolution_clock::now() >= std::get<1>(*item))
							{
								task();
								std::get<1>(*item) += std::get<2>(*item);
							}
// 						for (auto item = m_timer_elist.begin(); item != m_timer_elist.end();)
// 							(!std::get<0>(*item)) ? m_timer_elist.erase(item++) : item++;
					}
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
			//std::cout << "@@@ stop timer..." << std::endl;
			{
				std::lock_guard<std::mutex> locker(m_timer_mutex);
				m_timer_stopd = true;
				m_timer_condv.notify_one();
			}
		}).detach();
	}

	void stopd()
	{
		if (m_timer_stopd || m_try_to_stop)
			return;
		m_timing_time_point = std::chrono::high_resolution_clock::time_point(millis_t(0));
		m_try_to_stop = true;
		std::cout << "@@@ stop timer..." << std::endl;
		{
			std::unique_lock<std::mutex> locker(m_timer_mutex);
			m_timer_condv.wait(locker, [this] {return m_timer_stopd == true; });
			if (m_timer_stopd == true) {
				m_try_to_stop = false;
			}
		}
	}

	template<typename Func, typename... Args>
	void SyncWait(millis_t after, Func&& func, Args&&... args)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(after));
		std::forward<Func>(func)(std::forward<Args>(args)...);
	}

	template<typename Func, typename... Args>
	void AsynWait(millis_t after, Func&& func, Args&&... args)
	{
		std::thread([&]() {
			std::this_thread::sleep_for(std::chrono::milliseconds(after));
			std::forward<Func>(func)(std::forward<Args>(args)...);
		}).detach();
	}

private:
	std::atomic<bool>			m_timer_stopd{ 1 };
	std::atomic<bool>			m_try_to_stop{ 0 };
	std::mutex					m_timer_mutex;
	std::condition_variable		m_timer_condv;
	std::list<std::tuple<bool, std::chrono::high_resolution_clock::time_point, millis_t, std::function<void()>>>	m_timer_elist;
	std::chrono::high_resolution_clock::time_point			m_timing_time_point;
	std::chrono::high_resolution_clock::time_point			m_awaken_time_point;
};

void EchoFunc(std::string&& s) {
	std::cout << "test : " << s << std::endl;
}

int32_t print(std::string&& s, int32_t&& val) {
	std::cout << "@@@==" << s << " val=" << val << std::endl;
	return 0;
}
int fn(int) { return int(); }                            	// function 
template<typename T, typename... Arg> void wrapper(T&& arg, Arg&&... args)
{
	std::cout << sizeof...(args) << std::endl;	// arg,args 始终是左值
	print(std::forward<T>(arg), std::forward<Arg>(args)...); // 转发为左值或右值，依赖于 T
}

int32_t echo(std::string&& s) {
	std::cout << "@@@==" << s << std::endl;
	return 0;
}

template<typename Func, typename... Arg> void call_func(Func&& func, Arg&&... args)
{
	// using func_return_t = std::result_of<Func(Arg...)>::type;
	// std::cout << "func_return_t is int?: " << std::is_same<func_return_t, int32_t>::value << std::endl;
	// 注意：std::result_of<>::type = 返回值类型,type()是函数指针类型.
	// 具体type()的精确函数类型，当前是模板函数[==Func(Arg...)]类型，而非实例化的[!=int32_t(std::string&&)]类型。
#if 0
	// 完整类型推导
	using func_t = std::result_of<Func(Arg...)>::type();
	std::function<func_t> task(std::bind(std::forward<Func>(func), std::forward<Arg>(args)...));
	task();
	// 简化类型表达,但是可能会导致参数列表 右值引用 失败。
	//auto task = std::bind(std::forward<Func>(func), std::forward<Arg>(args)...);
	//task();
#else
	// 极简函数表达
	std::forward<Func>(func)(std::forward<Arg>(args)...);
#endif
}

static int print_device_sinks(AVOutputFormat *fmt, AVDictionary *opts)
{
	int ret, i;
	AVDeviceInfoList *device_list = NULL;

	if (!fmt || !fmt->priv_class || !AV_IS_OUTPUT_DEVICE(fmt->priv_class->category))
		return AVERROR(EINVAL);

	printf("Auto-detected sinks for %s:\n", fmt->name);
	if (!fmt->get_device_list) {
		ret = AVERROR(ENOSYS);
		printf("Cannot list sinks. Not implemented.\n");
		goto fail;
	}

	if ((ret = avdevice_list_output_sinks(fmt, NULL, opts, &device_list)) < 0) {
		printf("Cannot list sinks.\n");
		goto fail;
	}

	for (i = 0; i < device_list->nb_devices; i++) {
		printf("%s %s [%s]\n", device_list->default_device == i ? "*" : " ",
			device_list->devices[i]->device_name, device_list->devices[i]->device_description);
	}

fail:
	avdevice_free_list_devices(&device_list);
	return ret;
}

static int print_device_sources(AVInputFormat *fmt, AVDictionary *opts)
{
	int ret, i;
	AVDeviceInfoList *device_list = NULL;

	if (!fmt || !fmt->priv_class || !AV_IS_INPUT_DEVICE(fmt->priv_class->category))
		return AVERROR(EINVAL);

	printf("Auto-detected sources for %s:\n", fmt->name);
	if (!fmt->get_device_list) {
		ret = AVERROR(ENOSYS);
		printf("Cannot list sources. Not implemented.\n");
		goto fail;
	}

	if ((ret = avdevice_list_input_sources(fmt, NULL, opts, &device_list)) < 0) {
		printf("Cannot list sources.\n");
		goto fail;
	}

	for (i = 0; i < device_list->nb_devices; i++) {
		printf("%s %s [%s]\n", device_list->default_device == i ? "*" : " ",
			device_list->devices[i]->device_name, device_list->devices[i]->device_description);
	}

fail:
	avdevice_free_list_devices(&device_list);
	return ret;
}

int show_sources(/*void *optctx, const char *opt, const char *arg*/)
{
	AVInputFormat *fmt = NULL;
	char *dev = "";
	AVDictionary *opts = NULL;
	int ret = 0;
	int error_level = av_log_get_level();

	av_log_set_level(AV_LOG_ERROR);

// 	if ((ret = show_sinks_sources_parse_arg(arg, &dev, &opts)) < 0)
// 		goto fail;

	do {
		fmt = av_input_audio_device_next(fmt);
		if (fmt) {
			if (!strcmp(fmt->name, "lavfi"))
				continue; //it's pointless to probe lavfi
			if (dev && !av_match_name(dev, fmt->name))
				continue;
			print_device_sources(fmt, opts);
		}
	} while (fmt);
	do {
		fmt = av_input_video_device_next(fmt);
		if (fmt) {
			if (dev && !av_match_name(dev, fmt->name))
				continue;
			print_device_sources(fmt, opts);
		}
	} while (fmt);
fail:
	av_dict_free(&opts);
	//av_free(dev);
	av_log_set_level(error_level);
	return ret;
}

typedef struct AVStreamContext
{
	AVCodecContext *dec_ctx;
	AVCodecContext *enc_ctx;
} AVStreamContext;

typedef int32_t(*codecfunc_t)(AVCodecContext *avctx, AVFrame *picture, int32_t *got_picture_ptr, const AVPacket *avpkt);

#define check_return(x,y) if(x){ return y;}

bool
VideoCapture::start(AVCaptureOpts* cap_sets)
{
	if (!m_caputre_quit)
		return false;

	static std::once_flag oc;
	std::call_once(oc, [&]()
	{
		av_register_all();
		avfilter_register_all();
		avdevice_register_all();
		avformat_network_init();
	});

	if (cap_sets)
	{
		resolution = std::to_string(cap_sets->width) + std::string("x") + std::to_string(cap_sets->height);
		frame_rate = std::to_string(cap_sets->fps);
		pix_format = cap_sets->format_name;
		camera = std::string("video=") + cap_sets->camera_name;
		driver = cap_sets->driver;
	}
	show_CamerInfo();
	std::cout <<"resolution:" << resolution <<" frame_rate:" <<frame_rate << " pix_format:"  
		<< pix_format <<" camera:"<< camera << " driver:" << driver << std::endl;
	std::thread([&]()
	{
		m_caputre_quit = false;

		int32_t ret = -1, got_frame = 0;
		int32_t video_index = -1, audio_index = -1, stream_index = -1;
		codecfunc_t dec_func = NULL, enc_func = NULL;
		AVInputFormat   *ifmt = NULL;
		AVFormatContext *ifmt_ctx = NULL;
		AVDictionary    *opts = NULL;
		AVCodec			*codec = NULL;
		AVCodecContext	*codec_ctx = NULL;
		AVPacket		packet;
		//AVFrame			frame;
		//AVFrame			*pFrame = &frame;
		SwsContext *	m_swsctx = NULL;
		AVStreamContext *stream_ctx = NULL;

		std::shared_ptr<IVideoRender> p_render = IVideoRender::create(nullptr);
		av_dict_set(&opts, "video_size", resolution.c_str(), 0);
		av_dict_set(&opts, "framerate", frame_rate.c_str(), 0);
		av_dict_set(&opts, "pixel_format", pix_format.c_str(), 0);
		ifmt = av_find_input_format(driver.c_str());
		ret = avformat_open_input(&ifmt_ctx, camera.c_str(), ifmt, &opts);
		check_return(ret < 0, -1);
		av_dict_free(&opts);

		stream_ctx = (AVStreamContext*)av_malloc_array(ifmt_ctx->nb_streams, sizeof(AVStreamContext));
		check_return(!stream_ctx, -100);
		ret = avformat_find_stream_info(ifmt_ctx, NULL);
		check_return(ret < 0, -2);
		for (uint32_t i = 0; i < ifmt_ctx->nb_streams; ++i)
		{
			AVStream *stream = ifmt_ctx->streams[i];
			codec = avcodec_find_decoder(stream->codecpar->codec_id);
			codec_ctx = avcodec_alloc_context3(codec);
			avcodec_parameters_to_context(codec_ctx, stream->codecpar);
			if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
				codec_ctx->framerate = av_guess_frame_rate(ifmt_ctx, stream, NULL);
			ret = avcodec_open2(codec_ctx, codec, NULL);
			check_return(ret < 0, -3);
			video_index = (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) ? i : -1;
			audio_index = (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) ? i : -1;
			stream_ctx[i].dec_ctx = codec_ctx;
		}
		check_return(-1 == video_index && -1 == audio_index, -4);
		AVFrame *pFrame = nullptr;
		while (!m_caputre_quit)
		{
			if ((ret = av_read_frame(ifmt_ctx, &packet)) < 0)
				break;

			pFrame = av_frame_alloc();
			stream_index = packet.stream_index;
			int32_t type = ifmt_ctx->streams[stream_index]->codecpar->codec_type;
			//调试打印
			//debug_packet(ifmt_ctx, &packet, "@@input:");
			av_packet_rescale_ts(&packet, ifmt_ctx->streams[stream_index]->time_base,//1/90000|1/44100
				stream_ctx[stream_index].dec_ctx->time_base);//1/25(50)|1/44100，视频按帧进行解码（必须转换），音频按样本点进行解码（不转换也可）。	

			dec_func = (type == AVMEDIA_TYPE_VIDEO) ? avcodec_decode_video2 : avcodec_decode_audio4;
			if ((ret = dec_func(stream_ctx[stream_index].dec_ctx, pFrame, &got_frame, &packet)) < 0)
				break;

			if (got_frame)
			{
				pFrame->pts = pFrame->best_effort_timestamp;
				//debug_frames(stream_ctx[stream_index].dec_ctx, pFrame, "@@input:");
#if 1
				if (type == AVMEDIA_TYPE_VIDEO)
				{
					AVFrame* pfrm = av_rescale(&m_swsctx, pFrame,
						pFrame->width, pFrame->height, AV_PIX_FMT_YUV420P);
					if (nullptr != pfrm)
					{
						char* data = av_yuv420p_clone(pfrm);
						p_render->onVideoFrame(data, pFrame->width * pFrame->height * 3 / 2, pFrame->width, pFrame->height, pFrame->format);
						av_yuv420p_freep(data);// cahce av_frm data buffer.
						av_frame_free(&pfrm);
					}
				}
#endif
			}
			av_frame_free(&pFrame);
			av_packet_unref(&packet);
		}
		//av_packet_unref(&packet);
		av_frame_free(&pFrame);
		for (int i = 0; i < ifmt_ctx->nb_streams; i++) {
			avcodec_free_context(&stream_ctx[i].dec_ctx);
		}
		av_free(stream_ctx);
		avformat_close_input(&ifmt_ctx);
	}).detach();

	return true;
}

void
VideoCapture::stopd(void)
{
	m_caputre_quit = true;
}

int main()
{
	int32_t use_index = 1;
	CameraInfoForUsb *pCameraInfo = getCameraInfoForUsb();
	AVCaptureOpts opts;
	opts.camera_name = pCameraInfo->nCameraInfo[use_index].cameraName;
	opts.format_name = pCameraInfo->nCameraInfo[use_index].formatInfo[0].formatName;
	opts.fps = (int32_t)(0.5+1e7 / pCameraInfo->nCameraInfo[use_index].formatInfo[0].caps.MinFrameInterval);
	opts.width = pCameraInfo->nCameraInfo[use_index].formatInfo[0].caps.MinOutputSize.cx;
	opts.height = pCameraInfo->nCameraInfo[use_index].formatInfo[0].caps.MinOutputSize.cy;
	opts.driver = "dshow";
	AT::Timer timer;
	VideoCapture capture;
	for (int i=0;i<2; ++i)
	{
		bool ret = capture.start(&opts);
		printf("@@@ start caputre.... ret = %d\n", ret);
		std::this_thread::sleep_for(std::chrono::seconds(3));
		capture.stopd();
		std::this_thread::sleep_for(std::chrono::seconds(3));
	}
	bool ret = capture.start(nullptr);
	timer.elapsed(true);
	system("pause");
	return 0;
}