#pragma once

#include "IMediaCore.hpp"
#include <iostream>
#include <functional>
#include <chrono>
#include <future>
#include <cstdio>
#include <thread>
#include <mutex>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <ctime>

#ifdef WIN32
#include<windows.h>  
#include <direct.h>  
#include <io.h>
#include<shellapi.h> 
#endif
#include "vlc.h"

class MediaCore: public IMediaCore, public IMediaCoreObserver
{
public:
	MediaCore(IMediaCoreObserver* observer);
	~MediaCore();
public:
	/* 1.Set window and media to play */
	bool init(const std::string &media_name, void *win_hdle) override;
	void destroy() override;
	
	bool setCurMediaFile(const std::string &media_name) override;
	bool setRenderWindow(void *win_hdle) override;
	/* 2.Control media play actions.  */
	bool play() override;
	void stop() override;
	bool seek(int64_t seek_ms) override;
	void pause(bool do_pause) override;
	int64_t duration(const std::string &file="") override;//ms
	/* 3.Advanced media operations.  */
	void setMediaOutputDir(std::string &output_path) override;
	void startMediaCrop(std::string crop_input_file, std::vector<media_t> crop_output, std::string crop_output_name) override;
	void startMediaMerg(std::vector<std::string> merg_input_list, std::string merg_output_name) override;
	/* 4.Implement of oberver.*/
	void OnCurrentTimePoint(int64_t now_ms) override;
	void OnPlay()  override;
	void OnStop()  override;
	void OnSeek(int64_t seek_ms) override;
	void OnMediaCrop(STATUS err_code) override;
	void OnMediaMerg(STATUS err_code) override;
private:
	IMediaCoreObserver		 *m_observer	 = nullptr;
	/* libvlc module hdws    */
	libvlc_instance_t		 *m_vlc_instance = nullptr;
	libvlc_media_player_t	 *m_media_player = nullptr;
	libvlc_media_t			 *m_media		 = nullptr;
	/* mediacore play status */
	int64_t m_duration = 0;
	int64_t m_cur_time = 0;
	int64_t m_pre_time = 0;
	bool	m_is_pause			= false;
	bool    m_is_playing		= false;
	bool    m_is_exit			= false;
	bool    m_enable_transcode	= false;
	std::string	 m_app_tmp_dir  = "";
	std::string	 m_cur_play_file= "";
	std::thread  m_timer;
	/* advanced operations   */
	std::shared_ptr<int32_t> m_pmerg_status;
	std::shared_ptr<int32_t> m_pcrop_status;
};
