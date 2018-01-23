#pragma once

#if defined (WIN32) && defined (DLL_EXPORT)
# define MEDIACORE_API __declspec(dllexport)
#elif defined (__GNUC__) && (__GNUC__ >= 4)
# define MEDIACORE_API __attribute__((visibility("default")))
#else
# define MEDIACORE_API
#endif

#include <cstdio>
#include <vector>
#include <memory>
#include <thread>
#include <string>

typedef enum STATUS
{
	ENO_NOTSTART = 0,
	ENO_HANDLING,
	ENO_FINISHED,
	ENO_NOTEXIST,
	ENO_DOFFMPEG,
	ENO_UNKNOWND,
}STATUS;

typedef struct media_t
{
	std::pair<int32_t, int32_t> segment;//ms.
	std::string					segment_name;
}media_t;

class MEDIACORE_API IMediaCoreObserver
{
public:
	virtual void OnCurrentTimePoint(int64_t now_ms) = 0;
	virtual void OnPlay() = 0;
	virtual void OnStop() = 0;	
	virtual void OnSeek(int64_t seek_ms) = 0;
	virtual void OnMediaCrop(STATUS err_code) = 0;
	virtual void OnMediaMerg(STATUS err_code) = 0;
protected:
	virtual ~IMediaCoreObserver() = default;
};

typedef void(*log_callback)(int log_level, const char *format, ...);

class MEDIACORE_API  IMediaCore
{
public:
	static std::shared_ptr<IMediaCore> create(IMediaCoreObserver *observer);

	/* 1.Set window and media to play */
	virtual bool init(const std::string &media_name, void *win_hdle) = 0;
	virtual void destroy() = 0;

	virtual bool setCurMediaFile(const std::string &media_name) = 0;
	virtual bool setRenderWindow(void *win_hdle) = 0;
	/* 2.Control media play actions.  */
	virtual bool play() = 0;
	virtual void stop() = 0;
	virtual bool seek(int64_t seek_ms) = 0;
	virtual void pause(bool do_pause)  = 0;
	virtual int64_t duration()		   = 0;//ms

	/* 3.Advanced media operations.  */
	virtual void setMediaOutputDir(std::string &output_path) = 0;
	virtual void startMediaCrop(std::string crop_input_file, std::vector<media_t> crop_output_list, std::string crop_output_name) = 0;
	virtual void startMediaMerg(std::vector<std::string> merg_input_list, std::string merg_output_name) = 0;
protected:
	virtual ~IMediaCore() = default;
};

extern "C" MEDIACORE_API void setLogCallback(log_callback callback);
