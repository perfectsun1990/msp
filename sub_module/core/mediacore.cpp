
#include "mediacore.hpp"

# ifdef __cplusplus
extern "C" {
# endif
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswresample/swresample.h"
#include "libswscale/swscale.h"
#include "libavfilter/avfilter.h"
#include "libavutil/timestamp.h"
#include "libavformat/avformat.h"
# ifdef __cplusplus
}
# endif

log_callback g_log = nullptr;


void
setLogCallback(log_callback callback)
{
	g_log = callback;
}

static void
log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt, const char *tag)
{
	AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;
	printf("%s(%d/%d): iskey=%d,pts:%lld pts_time=%0.6g(s) dts:%lld dts_time=%0.6g(s) duration:%lld  stream_index:%d\n",
		tag, time_base->num, time_base->den, pkt->flags,
		pkt->pts, av_q2d(*time_base) * pkt->pts, pkt->dts, av_q2d(*time_base) * pkt->dts, pkt->duration, pkt->stream_index);
}


#ifndef DBG_OUT_API
#define DBG_OUT_API
#define dbg(fmt, ...)  do{ if( g_log!=nullptr ){ 							\
	g_log(400, "DBG:[%s:%d] " fmt , __FUNCTION__, __LINE__, ##__VA_ARGS__ );}else{\
	av_log( NULL, AV_LOG_DEBUG,		"DBG:[%s:%d] " fmt , __FUNCTION__, __LINE__, ##__VA_ARGS__ );}}while(0)
#define msg(fmt, ...)  do{ if( g_log!=nullptr ){ 							\
	g_log(300, "MSG:[%s:%d] " fmt , __FUNCTION__, __LINE__, ##__VA_ARGS__ );}else{\
	av_log( NULL, AV_LOG_INFO,		"MSG:[%s:%d] " fmt , __FUNCTION__, __LINE__, ##__VA_ARGS__ );}}while(0)
#define war(fmt, ...)  do{ if( g_log!=nullptr ){ 							\
	g_log(200, "WAR:[%s:%d] " fmt , __FUNCTION__, __LINE__, ##__VA_ARGS__ );}else{\
	av_log( NULL, AV_LOG_WARNING,	"WAR:[%s:%d] " fmt , __FUNCTION__, __LINE__, ##__VA_ARGS__ );}}while(0)
#define err(fmt, ...)  do{ if( g_log!=nullptr ){ 							\
	g_log(100, "ERR:[%s:%d] " fmt , __FUNCTION__, __LINE__, ##__VA_ARGS__ );}else{\
	av_log( NULL, AV_LOG_ERROR,		"ERR:[%s:%d] " fmt , __FUNCTION__, __LINE__, ##__VA_ARGS__ );}}while(0)
#endif
static bool
remuxing(std::string &src, std::string &dst);

static inline bool
cropMedia(std::string input_name, int32_t start_s, int32_t time_s, std::string output_dir, std::string output_name);
static inline bool
mergMedia(std::vector<std::string> merg_list, std::string output_dir, std::string output_name, int32_t fps, int32_t w, int32_t h);

static time_t
getTimeStamp()
{
	std::chrono::time_point<std::chrono::system_clock, std::chrono::microseconds> tp =
		std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::system_clock::now());
	return tp.time_since_epoch().count();
}

std::shared_ptr<IMediaCore>
IMediaCore::create(IMediaCoreObserver *observer)
{
	return std::make_shared<MediaCore>(observer);
}

MediaCore::MediaCore(IMediaCoreObserver* observer) :
	m_observer(observer),
	m_app_tmp_dir(""),
	m_cur_play_file("")
{
	const char * const vlc_args[] = {
		"--verbose=0", //be much more verbose then normal for debugging purpose
	};

	/* create a new LibVLC instance. */
	m_vlc_instance = libvlc_new(sizeof(vlc_args) / sizeof(vlc_args[0]), vlc_args);
	m_pmerg_status = std::make_shared<int32_t>(ENO_NOTSTART);
	m_pcrop_status = std::make_shared<int32_t>(ENO_NOTSTART);

	/* start timer */
	m_timer = std::thread([&, this](void)
	{
		while (!m_is_exit)
		{
			// update media current play params.
			if (m_media_player)
			{
				libvlc_media_t *curMedia = libvlc_media_player_get_media(m_media_player);
				if (curMedia == NULL)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
					continue;
				}
				m_cur_time = libvlc_media_player_get_time(m_media_player);
				OnCurrentTimePoint(m_cur_time);
				float pos = libvlc_media_player_get_position(m_media_player);
				int volume = libvlc_audio_get_volume(m_media_player);
				m_duration = libvlc_media_player_get_length(m_media_player);
			}

			// update media corp and merge status.
			OnMediaCrop((STATUS)*m_pcrop_status);
			OnMediaMerg((STATUS)*m_pmerg_status);

			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	});

	msg("meidacore is success created!");
}

MediaCore::~MediaCore()
{
	destroy();
	msg("meidacore is success destroyed!");
}

bool
MediaCore::init(const std::string &media_name, void *win_hdle)
{
	if (!setCurMediaFile(media_name))
		return false;

	if (!setRenderWindow(win_hdle))
		return false;
	return true;
}

void
MediaCore::destroy()
{
	if (!m_is_exit)
	{
		m_is_exit = true;
		m_timer.join();

		if (nullptr != m_media)
		{
			libvlc_media_release(m_media);
			m_media = nullptr;
		}
		if (nullptr != m_media_player)
		{
			libvlc_media_player_stop(m_media_player);
			/* Free the media_player */
			libvlc_media_player_release(m_media_player);
			m_media_player = nullptr;
		}
		if (nullptr != m_vlc_instance)
		{
			libvlc_release(m_vlc_instance);
			m_vlc_instance = nullptr;
		}
	}
}

bool
MediaCore::setCurMediaFile(const std::string &media_name)
{
	// recycling previous player.
	if (m_media_player)
	{
		libvlc_media_player_release(m_media_player);
		m_media_player = nullptr;
	}
	if (nullptr != m_media)
	{
		libvlc_media_release(m_media);
		m_media = nullptr;
	}

	if (m_vlc_instance != nullptr)
	{
		m_cur_play_file = media_name;
		m_media = libvlc_media_new_path(m_vlc_instance, m_cur_play_file.c_str());
		if (m_media != nullptr)
		{
			m_media_player = libvlc_media_player_new(m_vlc_instance);
			if (m_media_player != nullptr)
			{
				libvlc_media_player_set_media(m_media_player, m_media);
				return true;
			}
		}
	}
	err("[m_cur_play_file=%s,m_vlc_instance=%p,m_media=%p, m_media_player=%p] error!",
		m_cur_play_file.c_str(), (void*)m_vlc_instance, (void*)m_media, (void*)m_media_player);
	return false;
}

bool
MediaCore::setRenderWindow(void *win_hdle)
{
	if (m_vlc_instance != nullptr && nullptr != win_hdle)
	{
#if		defined(WIN32)
		libvlc_media_player_set_hwnd(m_media_player, win_hdle);
#elif	defined(APPLE)
		libvlc_media_player_set_nsobject(m_media_player, win_hdle);
#endif
		return true;
	}
	err("[m_cur_play_file=%s,m_vlc_instance=%p,m_media=%p, m_media_player=%p] error!",
		m_cur_play_file.c_str(), (void*)m_vlc_instance, (void*)m_media, (void*)m_media_player);
	return false;
}

bool
MediaCore::play()
{
	if (!m_is_playing
		&& m_media_player)
	{
		if (0 == libvlc_media_player_play(m_media_player))
		{
			int32_t retry_count = 0;
			do {
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				retry_count++;
			} while (duration() <= 0 || retry_count > 100);
			OnPlay();
			m_is_playing = true;
		}
	}
	pause(false);
	return false;
}

void
MediaCore::stop()
{
	if (m_is_playing
		&& m_media_player)
	{
		libvlc_media_player_stop(m_media_player);
		OnStop();
		m_is_playing = false;
	}
}

bool
MediaCore::seek(int64_t seek_ms)
{
	if (m_is_playing
		&& m_media_player)
	{
		libvlc_media_player_set_time(m_media_player, seek_ms);
		m_cur_time = libvlc_media_player_get_time(m_media_player);
		OnSeek(seek_ms);
		return true;
	}
	err("m_is_playing=%d[if you do seek,you must play first!], m_cur_play_file=%s,m_vlc_instance=%p,m_media=%p, m_media_player=%p] error!",
		m_is_playing, m_cur_play_file.c_str(), (void*)m_vlc_instance, (void*)m_media, (void*)m_media_player);
	return false;
}

void
MediaCore::pause(bool do_pause)
{
	m_is_pause = do_pause;
	if (m_media_player)
	{
		libvlc_media_player_set_pause(m_media_player, m_is_pause);
	}
}

int64_t
MediaCore::duration()
{
	AVFormatContext *ifmt_ctx = NULL;
	//AVPacket pkt;
	const char *in_filename = m_cur_play_file.data();
	int32_t stream_index = 0, ret = 0, i = 0;

	av_register_all();

	if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
		fprintf(stderr, "Could not open input file '%s'", in_filename);
		return false;
	}

	if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
		fprintf(stderr, "Failed to retrieve input stream information");
		return false;
	}
	//av_dump_format(ifmt_ctx, 0, in_filename, 0);

	return (ifmt_ctx->duration / 1000);
}

void
MediaCore::setMediaOutputDir(std::string &output_path)
{
	if (access(output_path.c_str(), 0))
	{
		if (mkdir(output_path.c_str()))
		{
			err("@@@specify an invalid output path=\"%s\"\n", output_path.c_str());
			return;
		}
	}

	std::string separator = "\\";
	if (!access(output_path.c_str(), 0) && output_path.size() > 2
		&& separator.compare(output_path.substr(output_path.size() - 1, output_path.size())))
		output_path = output_path + separator;
	m_app_tmp_dir = output_path;
	return;
}

void
MediaCore::startMediaCrop(std::string crop_input_file, std::vector<media_t> crop_output_list, std::string crop_output_name)
{
	std::string output_dir					= m_app_tmp_dir;
	std::shared_ptr<int32_t> pcrop_status	= m_pcrop_status;
	*pcrop_status = ENO_HANDLING;

	std::thread([=](void)
	{
		if (access(output_dir.c_str(), 0))
		{
			err("@@@invalid output path=\"%s\", you must call setMediaOutputDir first!\n", output_dir.c_str());
			*pcrop_status = ENO_NOTEXIST;
			return;
		}
#if 0
		// 输入文件多段裁剪
		for (uint32_t i = 0; i < crop_output_list.size(); ++i)
		{
			int32_t start_time = crop_output_list[i].segment.first;
			int32_t s_duration = crop_output_list[i].segment.second;

			if (output_list[i].segment_name.empty()) {
				output_list[i].segment_name = output_dir + std::tostring(start_time) + "_"
					+ std::tostring(start_time + s_duration) + "_" + std::tostring(getTimeStamp()) + "_segment.mp4";
			}
			else {
				output_list[i].segment_name = output_dir + output_list[i].segment_name;
			}

			if (!cropMedia(crop_input_file, start_time, s_duration, crop_output_list[i].segment_name))
			{
				err("Error: when cut[%d-%d], general %s failed!!\n",
					start_time, start_time + s_duration, crop_output_list[i].segment_name.c_str());
				*pcrop_status = ENO_DOFFMPEG;
				return;
			}
		}

		std::string output_file = output_dir + crop_output_name;
		// 合并生成一个文件
		if (!mergMedia(crop_output_list, crop_output_file, 0, 0, 0))
		{
			err("Error: general %s failed!!\n", crop_output_file.segment_name.c_str());
			*pcrop_status = ENO_DOFFMPEG;
			return;
		}

		// 是否删除分段文件
		if (0)
		{
			if (!access(crop_output_list[i].segment_name, 0))
			{
				if (remove(crop_output_list[i].segment_name))
				{
					war("can't remove %s!\n", crop_output_list[i].segment_name);
					continue;
				}
			}
		}
#else
		if (crop_output_list.size() > 1)
		{
			err("Hi! we current don't support multiple segments split!");
			*pcrop_status = ENO_UNKNOWND;
			return;
		}

		auto bgn = std::chrono::system_clock::now();
		for (uint32_t i = 0; i < crop_output_list.size(); ++i)
		{
			int32_t start_time = crop_output_list[i].segment.first / 1000;
			int32_t s_duration = crop_output_list[i].segment.second / 1000;
			if (!cropMedia(crop_input_file, start_time, s_duration, output_dir, crop_output_name))
			{
				err("error occured when crop on %s [%d,%d], general %s failed!\n",
					crop_input_file.c_str(), start_time, (start_time + s_duration), crop_output_name.c_str());
				*pcrop_status = ENO_DOFFMPEG;
				return;
			}
		}
		auto end = std::chrono::system_clock::now();
		std::chrono::duration<double> elaps_time = end - bgn;
		msg("cropMedia elaps_time= %llf s\n", elaps_time.count());
#endif
		*pcrop_status = ENO_FINISHED;
	}).detach();
	return;
}

void
MediaCore::startMediaMerg(std::vector<std::string> merg_input_list, std::string merg_output_name)
{
	std::string output_dir					= m_app_tmp_dir;
	std::shared_ptr<int32_t> pmerg_status	= m_pmerg_status;

	*pmerg_status = ENO_HANDLING;

	std::thread([=]()
	{
		if (access(output_dir.c_str(), 0))
		{
			if (mkdir(output_dir.c_str()))
			{
				*pmerg_status = ENO_NOTEXIST;
				err("Error: invalid merge output_dir=%s!\n", output_dir.c_str());
				return;
			}
		}
		auto bgn = std::chrono::system_clock::now();
		if (!mergMedia(merg_input_list, output_dir, merg_output_name, 0, 0, 0))
		{
			*pmerg_status = ENO_DOFFMPEG;
			err("Error:  when merge %s,general failed!\n", merg_output_name.c_str());
		}
		auto end = std::chrono::system_clock::now();
		std::chrono::duration<double> elaps_time = end - bgn;
		msg("mergMedia elaps_time= %llf s\n", elaps_time.count());
		*pmerg_status = ENO_FINISHED;
	}).detach();
}


void
MediaCore::OnCurrentTimePoint(int64_t now_ms)
{
	if (m_observer)
	{
		if (now_ms > 0)
			m_observer->OnCurrentTimePoint(now_ms);
	}
	return;
}

void
MediaCore::OnPlay()
{
	if (m_observer)
		m_observer->OnPlay();
	return;
}

void
MediaCore::OnStop()
{
	if (m_observer)
		m_observer->OnStop();
	return;
}

void
MediaCore::OnSeek(int64_t seek_ms)
{
	if (m_observer)
		m_observer->OnSeek(seek_ms);
	return;
}

void
MediaCore::OnMediaCrop(STATUS err_code)
{
	if (m_observer)
	{
		if (ENO_NOTSTART != *m_pcrop_status
			&&	ENO_HANDLING != *m_pcrop_status)
		{
			msg(" corp err_code=%d!\n", err_code);
			m_observer->OnMediaCrop(err_code);
			*m_pcrop_status = ENO_NOTSTART;
		}
	}
	return;
}

void
MediaCore::OnMediaMerg(STATUS err_code)
{
	if (m_observer)
	{
		if (ENO_NOTSTART != *m_pmerg_status
			&&	ENO_HANDLING != *m_pmerg_status)
		{
			msg("merge err_code=%d!\n", err_code);
			m_observer->OnMediaMerg(err_code);
			*m_pmerg_status = ENO_NOTSTART;
		}
	}
	return;
}

//////////////////////////////////////////////////////////////////////////
#ifdef WIN32

static int
Utf82Unicode(const char* utf8, wchar_t* unicode, unsigned int nBuffSize)
{
	if (!utf8 || !strlen(utf8)) {
		return 0;
	}
	int dwUnicodeLen = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
	size_t num = dwUnicodeLen * sizeof(wchar_t);
	if (num > nBuffSize) {
		return 0;
	}
	MultiByteToWideChar(CP_UTF8, 0, utf8, -1, unicode, dwUnicodeLen);
	return dwUnicodeLen;
}

static int
Unicode2Utf8(const wchar_t* unicode, char* utf8, int nBuffSize)
{
	if (!unicode || !wcslen(unicode)) {
		return 0;
	}
	int len;
	len = WideCharToMultiByte(CP_UTF8, 0, unicode, -1, NULL, 0, NULL, NULL);
	if (len > nBuffSize) {
		return 0;
	}
	WideCharToMultiByte(CP_UTF8, 0, unicode, -1, utf8, len, NULL, NULL);
	return len;
}

static int
MultiByte2Utf8(const char *mult, char* utf8, int nBuffSize)
{
	if (nullptr == mult || nullptr == utf8)
		return 0;
	//返回接受字符串所需缓冲区的大小，已经包含字符结尾符'\0',iSize =wcslen(srcString)+1
	int iSize = MultiByteToWideChar(CP_ACP, 0, mult, -1, NULL, 0);
	wchar_t* pUnicode = (wchar_t *)malloc(iSize * sizeof(wchar_t));
	MultiByteToWideChar(CP_ACP, 0, mult, -1, pUnicode, iSize);
	int len;
	len = WideCharToMultiByte(CP_UTF8, 0, pUnicode, -1, NULL, 0, NULL, NULL);
	if (len > nBuffSize) {
		return 0;
	}
	WideCharToMultiByte(CP_UTF8, 0, pUnicode, -1, utf8, len, NULL, NULL);
	free(pUnicode);
	return len;
}

static std::wstring
string2wstring(const std::string& str)
{
	LPCSTR pszSrc = str.c_str();
	int nLen = MultiByteToWideChar(CP_ACP, 0, pszSrc, -1, NULL, 0);
	if (nLen == 0)
		return std::wstring(L"");

	wchar_t* pwszDst = new wchar_t[nLen];
	if (!pwszDst)
		return std::wstring(L"");

	MultiByteToWideChar(CP_ACP, 0, pszSrc, -1, pwszDst, nLen);
	std::wstring wstr(pwszDst);
	delete[] pwszDst;
	pwszDst = NULL;

	return wstr;
}

static std::string
wstring2string(const std::wstring& wstr)
{
	LPCWSTR pwszSrc = wstr.c_str();
	int nLen = WideCharToMultiByte(CP_ACP, 0, pwszSrc, -1, NULL, 0, NULL, NULL);
	if (nLen == 0)
		return std::string("");

	char* pszDst = new char[nLen];
	if (!pszDst)
		return std::string("");

	WideCharToMultiByte(CP_ACP, 0, pwszSrc, -1, pszDst, nLen, NULL, NULL);
	std::string str(pszDst);
	delete[] pszDst;
	pszDst = NULL;

	return str;
}

static inline void
replace_all(std::string& str, char* oldValue, char* newValue)
{
	std::string::size_type pos(0);

	while (true)
	{
		pos = str.find(oldValue, pos);
		if (pos != (std::string::npos))
		{
			str.replace(pos, strlen(oldValue), newValue);
			pos += 2;//注意是加2，为了跳到下一个反斜杠  
		}
		else {
			break;
		}
	}
}

static bool
shellExecuteCommand(std::string cmd, std::string arg, bool is_show)
{
	std::wstring w_cmd = string2wstring(cmd);
	std::wstring w_arg = string2wstring(arg);

	SHELLEXECUTEINFO ShExecInfo;
	ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
	ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
	ShExecInfo.hwnd = NULL;
	ShExecInfo.lpVerb = NULL;
	ShExecInfo.lpFile = w_cmd.c_str(); //can be a file as well  
	ShExecInfo.lpParameters = w_arg.c_str();
	ShExecInfo.lpDirectory = NULL;
	ShExecInfo.nShow = 0;
	ShExecInfo.hInstApp = NULL;
	BOOL ret = ShellExecuteEx(&ShExecInfo);
	WaitForSingleObject(ShExecInfo.hProcess, INFINITE);
	DWORD dwCode = 0;
	GetExitCodeProcess(ShExecInfo.hProcess, &dwCode);
	if (dwCode)
		err("shellExecuteCommand: [%s %s <ErrorCode=%d>] error!\n", cmd.c_str(), arg.c_str(), dwCode);
	CloseHandle(ShExecInfo.hProcess);
	return !dwCode;
}
#else

#endif

static inline bool
cropMedia(std::string input_name, int32_t start_s, int32_t time_s, std::string output_dir, std::string output_name)
{
#if 1
	std::string tmp = output_dir + std::to_string(start_s) + "_" + std::to_string(start_s + time_s) + "_" + std::to_string(getTimeStamp()) + "_tmp.mp4";
	std::string tmp_1 = output_dir + std::to_string(start_s) + "_" + std::to_string(start_s + time_s) + "_" + std::to_string(getTimeStamp()) + "repair_tmp.mp4";
	// crop media file in second.
	std::string cmd = "ffmpeg.exe";
	std::string arg = " -ss " + std::to_string(start_s) + " -t " + std::to_string(time_s) + " -accurate_seek -i " + input_name + " -c copy " + "  -y " + tmp;
	if (!shellExecuteCommand(cmd, arg, false))
		return false;

	// repair output media ts.
	arg = " -i " + tmp + " -c copy " + "  -y " + tmp_1;
	if (!shellExecuteCommand(cmd, arg, false))
		return false;
	// repair output pkt duration.
	if (!remuxing(tmp_1, output_dir+output_name))
		return false;

	if (!access(tmp.c_str(), 0))
		remove(tmp.c_str());
	if (!access(tmp_1.c_str(), 0))
		remove(tmp_1.c_str());
	return true;

#else// 精确裁剪+后续使用api实现。
	AVFormatContext *ifmt_ctx = NULL;
	AVPacket pkt;
	const char *in_filename = input_name.data();
	int32_t stream_index = 0, ret = 0, i = 0;

	av_register_all();

	if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
		fprintf(stderr, "Could not open input file '%s'", in_filename);
		return false;
	}

	if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
		fprintf(stderr, "Failed to retrieve input stream information");
		return false;
	}
	av_dump_format(ifmt_ctx, 0, in_filename, 0);
	std::vector<double> IntraTimeStampMap;
	while (1)
	{
		ret = av_read_frame(ifmt_ctx, &pkt);
		if (ret < 0) break;

		AVStream *in_stream = ifmt_ctx->streams[pkt.stream_index];
		if (in_stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			if (1 == pkt.flags)
			{
				AVRational *time_base = &ifmt_ctx->streams[pkt.stream_index]->time_base;
				IntraTimeStampMap.push_back(av_q2d(*time_base) * pkt.pts);
			}
		}
		else if (in_stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			//log_packet(ifmt_ctx, &pkt, "audio:");
		}
		av_packet_unref(&pkt);
	}

	start_s = 15, time_s = 10;

	//获取start_s最近的前一个I帧的时间戳。
	double duration = (double)ifmt_ctx->duration / 1000000;
	double t0 = 0xFFFFFFFF, t1 = 0xFFFFFFFF, start = (double)start_s, last_time = (double)time_s;

	// 	if ( start + last_time > duration )
	// 	{
	// 		printf("Error: start + last_time=%llf is out of range,duration=%llf!\n", start + last_time, duration);
	// 		return false;
	// 	}

	for (int32_t i = 0; i < IntraTimeStampMap.size(); ++i)
	{
		printf("----%0.6g %0.6g\n", IntraTimeStampMap[i], IntraTimeStampMap[IntraTimeStampMap.size() - 1]);

		// I很少，后面全是BP帧.
		if (start >= IntraTimeStampMap[IntraTimeStampMap.size() - 1])
		{
			t0 = IntraTimeStampMap[IntraTimeStampMap.size() - 1];
			t1 = duration;

			printf("1find: start=%0.6g,Itra=(%0.6g, %0.6g)\n", start, t0, t1);
			// 直接拷贝裁剪最后一个Gop组，然后转码裁剪 Gop中的start-end的部分。

			//  1.获取 start-t1 部分。
			//  拷贝裁剪t0-t1
			//  ffmpeg -i ./src.mp4 -ss 00:00:t0 -t t1-t0 -c copy t0_t1.mp4
			std::string tmp = std::to_string((int32_t)t0) + "_" + std::to_string((int32_t)(t1 - t0)) + ".mp4";
			std::string cmd = "ffmpeg.exe";
			std::string arg = " -i " + input_name + " -ss " + std::to_string((int32_t)t0) + " -t " + std::to_string((int32_t)(t1 - t0))
				+ "" + "  -y " + tmp;
			if (!shellExecuteCommand(cmd, arg, false))
				return false;

			//  在t0-t1基础上，转码裁剪start-end<start_s+time_s>
			//  ffmpeg -i t0_t1.mp4 -ss 00:00:[start-t0] -t [last_time] start_end.mp4
			arg = " -i " + tmp + " -ss " + std::to_string((int32_t)(start - t0)) + " -t " + std::to_string((int32_t)(last_time))
				+ "" + "  -y " + output_name;
			if (!shellExecuteCommand(cmd, arg, false))
				return false;

			if (!access(tmp.c_str(), 0))
				remove(tmp.c_str());

			break;
		}

		// 针对从头开始的截取动作，直接从头截取就好了，不必细化，开头肯定有I帧数据。
		if (start < IntraTimeStampMap[0])
		{
			printf("2find: start=%0.6g,Itra=(%0.6g, %0.6g)\n", start, t0, t1);
			// 直接拷贝裁剪start-end部分媒体。
			//  1.获取 start-end 部分。
			//  拷贝裁剪start-end<start_s+time_s>
			//  ffmpeg -i ./src.mp4 -ss 00:00:start_s -t time_s -c copy start_end.mp4
			std::string cmd = "ffmpeg.exe";
			std::string arg = " -i " + input_name + " -ss " + std::to_string((int32_t)start) + " -t " + std::to_string((int32_t)last_time)
				+ " -c copy " + "  -y " + output_name;
			return shellExecuteCommand(cmd, arg, false);
			break;
		}

		// 普通的情况，start 位于两个I帧之间。
		if (start > IntraTimeStampMap[i]
			&& start <= IntraTimeStampMap[i + 1])
		{
			t0 = IntraTimeStampMap[i];
			t1 = IntraTimeStampMap[i + 1];
			printf("3find: start=%0.6g,Itra=(%0.6g, %0.6g)\n", start, t0, t1);
			if (start + last_time <= t1)
			{
				//  1.获取 start-t1 部分。
				//  拷贝裁剪t0-t1
				//  ffmpeg -i ./src.mp4 -ss 00:00:t0 -t t1-t0 -c copy t0_t1.mp4
				//  在t0-t1基础上，转码裁剪start-end<start_s+time_s>
				//  ffmpeg -i t0_t1.mp4 -ss 00:00:[start-t0] -t [end-start] start_end.mp4

				//  1.获取 start-t1 部分。
				//  拷贝裁剪t0-t1
				//  ffmpeg -i ./src.mp4 -ss 00:00:t0 -t t1-t0 -c copy t0_t1.mp4
				std::string tmp = std::to_string((int32_t)t0) + "_" + std::to_string((int32_t)(t1 - t0)) + ".mp4";
				std::string cmd = "ffmpeg.exe";
				std::string arg = " -i " + input_name + " -ss " + std::to_string((int32_t)t0) + " -t " + std::to_string((int32_t)(t1 - t0))
					+ " -c copy " + "  -y " + tmp;
				if (!shellExecuteCommand(cmd, arg, false))
					return false;

				//  在t0-t1基础上，转码裁剪start-end<start_s+time_s>
				//  ffmpeg -i t0_t1.mp4 -ss 00:00:[start-t0] -t [last_time] start_end.mp4
				arg = " -i " + tmp + " -ss " + std::to_string((int32_t)(start - t0)) + " -t " + std::to_string((int32_t)(last_time))
					+ "" + "  -y " + output_name;
				if (!shellExecuteCommand(cmd, arg, false))
					return false;

				if (!access(tmp.c_str(), 0))
					remove(tmp.c_str());
			}
			else {
				//  1.获取 start-t1 部分。
				//  拷贝裁剪t0-t1
				//  ffmpeg -i ./src.mp4 -ss 00:00:t0 -t t1-t0 -c copy t0_t1.mp4			
				std::string tmp_1 = std::to_string((int32_t)t0) + "_" + std::to_string((int32_t)(t1 - t0)) + "_1.mp4";
				std::string tmp_1_1 = std::to_string((int32_t)start) + "_" + std::to_string((int32_t)(t1 - start)) + "_1.mp4";
				std::string tmp_2 = std::to_string((int32_t)t1) + "_" + std::to_string((int32_t)(start + last_time - t1)) + "_2.mp4";

				std::string cmd = "ffmpeg.exe";
				std::string arg = " -i " + input_name + " -ss " + std::to_string((int32_t)(t0)) + " -t " + std::to_string((int32_t)(t1 - t0))
					+ " -c copy " + "  -y " + tmp_1;
				if (!shellExecuteCommand(cmd, arg, false))
					return false;
				//  在t0-t1基础上，转码裁剪start-t1
				//  ffmpeg -i t0_t1.mp4 -ss 00:00:[start-t0] -t [t1-start] start_t1.mp4
				arg = " -i " + tmp_1 + " -ss " + std::to_string((int32_t)(start - t0 + 1)) + " -t " + std::to_string((int32_t)(t1 - start))
					+ "" + "  -y " + tmp_1_1;
				if (!shellExecuteCommand(cmd, arg, false))
					return false;

				//  2.获取t1-end<start_s+time_s>部分。
				//  拷贝裁剪end(start_s+time_s )-t1
				//  ffmpeg -i ./src.mp4 -ss 00:00:t1 -t [(start_s+time_s )-t1] -c copy t1_end.mp4
				arg = " -i " + input_name + " -ss " + std::to_string((int32_t)(t1)) + " -t " + std::to_string((int32_t)(start + last_time - t1 + 5))
					+ " -c copy " + "  -y " + tmp_2;
				if (!shellExecuteCommand(cmd, arg, false))
					return false;

				// 3.合并-未做完。
				// ffmpeg -f concat -safe 0 -i merge.list -c copy start_end.mp4
				arg = " -i " + tmp_1_1 + " -i " + tmp_2 + "  -f mp4  " + " -c copy " + "  -y " + output_name;
				if (!shellExecuteCommand(cmd, arg, false))
					return false;

				if (!access(tmp_1.c_str(), 0))
					remove(tmp_1.c_str());
				if (!access(tmp_2.c_str(), 0))
					remove(tmp_2.c_str());
				if (!access(tmp_1_1.c_str(), 0))
					remove(tmp_1_1.c_str());

			}
			break;
		}
	}
	avformat_close_input(&ifmt_ctx);
#endif
}

static inline bool
mergMedia(std::vector<std::string> merg_list, std::string output_dir, std::string output_name, int32_t fps, int32_t w, int32_t h)
{
	AVFormatContext *ifmt_ctx = NULL;
	int32_t stream_index = 0, ret = 0;

	if (merg_list.empty())
	{
		err("Error empty input merge media list!");
		return false;
	}

	const char *in_filename = merg_list[0].data();
	std::string  output_file = output_dir + output_name;
	// check and revise transcode params.
	if (0 == w && 0 == h && 0 == fps)
	{
		av_register_all();

		if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0)
		{
			err("avformat_open_input: [%s] error!", in_filename);
			return false;
		}

		if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0)
		{
			err("avformat_find_stream_info: [retrieve input stream information] error!");
			return false;
		}
		av_dump_format(ifmt_ctx, 0, in_filename, 0);

		for (uint32_t i = 0; i < ifmt_ctx->nb_streams; i++)
		{
			if (AVMEDIA_TYPE_VIDEO == ifmt_ctx->streams[i]->codecpar->codec_type)
			{
				w = ifmt_ctx->streams[i]->codecpar->width;
				h = ifmt_ctx->streams[i]->codecpar->height;
				fps = ifmt_ctx->streams[i]->avg_frame_rate.num / ifmt_ctx->streams[i]->avg_frame_rate.den;
			}
		}
	}

	// general output media's merge list.
	std::string merge_cfg_file = output_file + std::to_string(getTimeStamp()) + "_merge.list";
	FILE * fp = fopen(merge_cfg_file.c_str(), "w+");
	if (nullptr == fp) {
		err("fopen: [%s] error!", merge_cfg_file.c_str());
		return false;
	}

	std::time_t now_ts = getTimeStamp();

	for (uint32_t i = 0; i < merg_list.size(); ++i)
	{
		std::string segment_file;
		// 1.transcode files.
		if (0)
		{
			segment_file = output_file + "_" + std::to_string(i) + "_tmp.mp4";
			std::string cmd = "ffmpeg.exe";
			std::string arg = " -i " + merg_list[i] + " -preset veryfast -qscale 0 " + " -r " + std::to_string(fps) + \
				" -s " + std::to_string(w) + "x" + std::to_string(h) + " -vcodec h264 -acodec aac " + " -y " + segment_file;

			if (!shellExecuteCommand(cmd, arg, false))
				return false;
		}
		else {
			segment_file = output_file + "_" + std::to_string(i) + std::to_string(now_ts) + "_tmp.ts";
			std::string cmd = "ffmpeg.exe";
			std::string arg = " -i " + merg_list[i] + " -preset veryfast " + " -c copy " + " -y " + segment_file;
			if (!shellExecuteCommand(cmd, arg, false))
				return false;
		}

		// 2.update merge.list	
		std::string merge_list_item = "file " + segment_file + "\n";
		replace_all(merge_list_item, "\\", "\\\\");
		if (fwrite(merge_list_item.c_str(), sizeof(char), merge_list_item.size(), fp) < 0) {
			err("fwrite: [%s] error!", merge_list_item.c_str());
			return false;
		}
		fflush(fp);
		std::cout << merge_list_item << std::endl;
	}
	fclose(fp);

	//ffmpeg - f concat - safe 0 - i merge.list - c copy - y merge.flv
	std::string cmd = "ffmpeg.exe";
	std::string arg = " -f concat -safe 0 -i " + merge_cfg_file + " -c copy -y " + output_file;
	if (!shellExecuteCommand(cmd, arg, false))
		return false;

	// remove temporary transcode files.
	for (uint32_t i = 0; i < merg_list.size(); ++i)
	{
		std::string segment_file = output_file + "_" + std::to_string(i) + std::to_string(now_ts) + "_tmp.ts";
		if (!access(segment_file.c_str(), 0))
			remove(segment_file.c_str());
	}
	if (!access(merge_cfg_file.c_str(), 0))
		remove(merge_cfg_file.c_str());
	return true;
}


static bool
remuxing(std::string &src, std::string &dst)
{
#define  MAX_NAME_SIZE 512
	AVOutputFormat *ofmt = NULL;
	AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
	AVPacket pkt;
	int ret, i;
	int stream_index = 0;
	int *stream_mapping = NULL;
	int stream_mapping_size = 0;

	const char *in_filename = src.c_str();
	const char *out_filename = dst.c_str();

	av_register_all();

	if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
		fprintf(stderr, "Could not open input file '%s'", in_filename);
		goto end;
	}

	if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
		fprintf(stderr, "Failed to retrieve input stream information");
		goto end;
	}

	//av_dump_format(ifmt_ctx, 0, in_filename, 0);

	avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_filename);
	if (!ofmt_ctx) {
		fprintf(stderr, "Could not create output context\n");
		ret = AVERROR_UNKNOWN;
		goto end;
	}

	stream_mapping_size = ifmt_ctx->nb_streams;
	stream_mapping = (int*)av_mallocz_array(stream_mapping_size, sizeof(*stream_mapping));
	if (!stream_mapping) {
		ret = AVERROR(ENOMEM);
		goto end;
	}

	ofmt = ofmt_ctx->oformat;

	for (i = 0; i < ifmt_ctx->nb_streams; i++)
	{
		AVStream *out_stream;
		AVStream *in_stream = ifmt_ctx->streams[i];
		AVCodecParameters *in_codecpar = in_stream->codecpar;

		if (in_codecpar->codec_type != AVMEDIA_TYPE_AUDIO &&
			in_codecpar->codec_type != AVMEDIA_TYPE_VIDEO &&
			in_codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE)
		{
			stream_mapping[i] = -1;
			continue;
		}

		stream_mapping[i] = stream_index++;

		out_stream = avformat_new_stream(ofmt_ctx, NULL);
		if (!out_stream) {
			fprintf(stderr, "Failed allocating output stream\n");
			ret = AVERROR_UNKNOWN;
			goto end;
		}

		ret = avcodec_parameters_copy(out_stream->codecpar, in_codecpar);
		if (ret < 0) {
			fprintf(stderr, "Failed to copy codec parameters\n");
			goto end;
		}
		out_stream->codecpar->codec_tag = 0;
	}
	av_dump_format(ofmt_ctx, 0, out_filename, 1);

	if (!(ofmt->flags & AVFMT_NOFILE)) {
		ret = avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
		if (ret < 0) {
			fprintf(stderr, "Could not open output file '%s'", out_filename);
			goto end;
		}
	}

	ret = avformat_write_header(ofmt_ctx, NULL);
	if (ret < 0) {
		fprintf(stderr, "Error occurred when opening output file\n");
		goto end;
	}

	while (1)
	{
		AVStream *in_stream, *out_stream;

		ret = av_read_frame(ifmt_ctx, &pkt);
		if (ret < 0)
			break;

		in_stream = ifmt_ctx->streams[pkt.stream_index];
		if (pkt.stream_index >= stream_mapping_size ||
			stream_mapping[pkt.stream_index] < 0) {
			av_packet_unref(&pkt);
			continue;
		}

		pkt.stream_index = stream_mapping[pkt.stream_index];
		out_stream = ofmt_ctx->streams[pkt.stream_index];

		//log_packet(ifmt_ctx, &pkt, "input:");

		/* copy packet */
		pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
		pkt.pos = -1;

		if (in_stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
			if (pkt.duration > 1024 * 2) {
				pkt.duration = 1024;
			}
		}
		if (in_stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			if (pkt.duration > 3600 * 2) {
				pkt.duration = 3600;
			}
		}
		//log_packet(ofmt_ctx, &pkt, "output:");

		//截取一段时间的视频.
		ret = av_interleaved_write_frame(ofmt_ctx, &pkt);
		if (ret < 0) {
			fprintf(stderr, "Error muxing packet\n");
			break;
		}

		av_packet_unref(&pkt);
	}

	av_write_trailer(ofmt_ctx);
end:

	avformat_close_input(&ifmt_ctx);

	/* close output */
	if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
		avio_closep(&ofmt_ctx->pb);
	avformat_free_context(ofmt_ctx);

	av_freep(&stream_mapping);

	if (ret < 0 && ret != AVERROR_EOF) {
		//printf("Error occurred: %s\n", av_err2str(ret));
		return false;
	}

	return true;
}

#ifdef MEDIACORE_TEST
int main()
{
#if 0
	std::cout << "system clock          : ";
	std::cout << std::chrono::system_clock::period::num << "/" << std::chrono::system_clock::period::den << "s" << std::endl;
	std::cout << "steady clock          : ";
	std::cout << std::chrono::steady_clock::period::num << "/" << std::chrono::steady_clock::period::den << "s" << std::endl;
	std::cout << "high resolution clock : ";
	std::cout << std::chrono::high_resolution_clock::period::num << "/" << std::chrono::high_resolution_clock::period::den << "s" << std::endl;

	std::cout << "ts=" << getTimeStamp() << "time=" << time(0) << std::endl;

	auto start = std::chrono::system_clock::now();
	std::cout << "f(12) = " << fibonacci(42) << '\n';
	auto end = std::chrono::system_clock::now();

	std::chrono::duration<double> elapsed_seconds = end - start;
	std::time_t end_time = std::chrono::system_clock::to_time_t(end);
	std::cout << "finished computation at " << std::ctime(&end_time)
		<< "elapsed time: " << elapsed_seconds.count() << "s\n";
	std::string src = "E:\\test\\remuxing.mp4";
	std::string dst = "E:\\test\\remuxing-out.mp4";

	remuxing(src, dst);
#else
	int count = 0;
	while (true)
	{
		std::shared_ptr<IMediaCore> p = IMediaCore::create(nullptr);
		std::string test_file;
		if (count++ % 2) {
			test_file = "E:\\av-test\\中-crop.mp4";
		}
		else {
			test_file = "E:\\av-test\\中-crop.mp4"; // "E:\\test\\狐狸精MTV-罗志祥.wmv";
		}

		// 中文转码-针对vlc播放必须的。
		char media_file[512] = { 0 };
		MultiByte2Utf8(test_file.data(), media_file, sizeof(media_file));

		p->init(media_file, ::GetDesktopWindow());
		p->play();

#if 1
		p->setMediaOutputDir(std::string("E:\\剪辑输出"));
		// media crop test....
		std::vector<media_t> crop_output_list;
		std::pair<int32_t, int32_t> seg_1(10000, 20000);
		media_t m_crop_item_1 = { seg_1, "" };
		crop_output_list.push_back(m_crop_item_1);
		std::string crop_input_file = "E:\\av-test\\8.mp4";
		std::string crop_output_name = std::to_string(seg_1.first) + "_" +
			std::to_string(seg_1.first + seg_1.second) + "_" + std::to_string(getTimeStamp()) + "中文_crop.mp4";
		p->startMediaCrop(crop_input_file, crop_output_list, crop_output_name);

		// media merge test....
		std::string m_f1 = "E:\\av-test\\中-merge-part1.mp4";
		std::string m_f2 = "E:\\av-test\\中-merge-part2.mp4";
		std::vector<std::string> merg_input_list;
		merg_input_list.push_back(m_f1);
		merg_input_list.push_back(m_f2);
		std::string merg_output_name = std::to_string(getTimeStamp()) + "中文_merge.mp4";
		p->startMediaMerg(merg_input_list, merg_output_name);
#endif
		std::this_thread::sleep_for(std::chrono::seconds(5));
		p->stop();
		p->destroy();
		p.reset();
		std::this_thread::sleep_for(std::chrono::seconds(600));
	}
#endif
	system("pause");
	return 0;
}
#endif
//////////////////////////////////////////////////////////////////////////