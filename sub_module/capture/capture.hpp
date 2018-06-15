
#pragma  once

#include <vector>
#include <string>
#include <atomic>
#include <iostream>

#ifdef  WIN32
#include "dshow.hpp"
CameraInfoForUsb *getCameraInfoForUsb();
#endif

typedef struct AVCaptureOpts
{
	std::string				    driver{ "" };
	std::string				    camera_name{ "" };
	std::string				    format_name{ "" };
	int32_t						fps{ 0 };
	int32_t						width{ 0 };
	int32_t						height{ 0 };
};

class VideoCapture 
{
public:
	bool start(AVCaptureOpts* cap_sets);
	void stopd(void);
private:
	std::atomic_bool m_caputre_quit{true};
	std::string resolution{ "" };
	std::string frame_rate{ "" };
	std::string pix_format{ "" };
	std::string camera{ "" };
	std::string driver{ "dshow" };
};


#if 0

typedef struct AVCaptureOpts
{
	int32_t						avformat{ 0 };
	int32_t						max_vfps{ 0 };
	int32_t						min_vfps{ 0 };
	int32_t						pix_with{ 0 };
	int32_t						pix_high{ 0 };
	int32_t						max_chnn{ 0 };
	int32_t						min_chnn{ 0 };
	int32_t						max_rate{ 0 };
	int32_t						min_rate{ 0 };
	int32_t						max_bits{ 0 };
	int32_t						min_bits{ 0 };
}AVCaptureOpts;
typedef struct AVCaptureInfo
{
	std::string					name{ "" };	//capture name.
	std::vector<AVCaptureOpts>	opts;
}AVCaptureInfo;
typedef struct AVCaptureSets
{
	int32_t						type{ 0 };
	int32_t						nums{ 0 };
	std::vector<AVCaptureInfo>	info;
}AVCaptureSets;
typedef struct PeripherySets
{
	std::string					divr{ "" };	//dshow,  v4l2.
	AVCaptureSets				acap;
	AVCaptureSets				vcap;
	PeripherySets*				next{ nullptr };
}PeripherySets;

class ICaptureObserver
{
public:
	virtual	void  onMFrm(std::shared_ptr<MRframe> avfrm) = 0;
	// 	virtual void onCaptureStart(void) = 0;
	// 	virtual void onCaptureStopd(void) = 0;
	// 	virtual void onCapturePause(void) = 0;
};

class ICapture
{
public:
	/* configs functions. */
	virtual void	update(void* config) = 0;
	virtual void	config(void* config) = 0;
	virtual STATUS	status(void) = 0;
	virtual int32_t Q_size(void) = 0;
	virtual int32_t cached(void) = 0;// driver cached size.
	virtual	void    onMFrm(std::shared_ptr<MRframe> avfrm) = 0;
	/* control functions. */
	virtual void	start(void) = 0;
	virtual void	stopd(bool stop_quik = false) = 0;
	virtual void	pause(bool pauseflag = false) = 0;
protected:
	virtual ~ICapture() = default;
};
#endif

