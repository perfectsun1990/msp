
#include "pubcore.hpp"


#define AV_SYNC_THRESHOLD					0.01
#define AV_NOSYNC_THRESHOLD					10.0

typedef struct MSynConfig
{
	bool									pauseflag{ false };
	int32_t									sync_type{ 0 };
}MSynConfig;

class ISynchroObserver
{// default use ffmpeg.
public:
	virtual void onSync(std::shared_ptr<MRframe> av_frm) = 0;
// 	virtual void onSynchroStart(void) = 0;
// 	virtual void onSynchroStopd(void) = 0;
// 	virtual void onSynchroPause(void) = 0;
// 	virtual void onSynchroSeekp(void) = 0;
};

class ISynchro
{
public:
	static std::shared_ptr<ISynchro> create(
		std::shared_ptr<ISynchroObserver> observer = nullptr);
	/* configs functions. */
	virtual void	update(void* config) = 0;
	virtual void	config(void* config) = 0;
	virtual STATUS	status(void) = 0;
	virtual int32_t Q_size(void) = 0;
	virtual void	updateSyncType(int32_t sync_type)	   = 0;
	virtual	void    onMFrm(std::shared_ptr<MRframe> avfrm) = 0;
	/* control functions. */
	virtual void	start(void)  = 0;
	virtual void	stopd(bool stop_quik = false) = 0;
	virtual void	pause(bool pauseflag = false) = 0;
protected:
	virtual ~ISynchro() = default;
};

//媒体同步机
class MediaSynchro : public ISynchro
{
public:
	MediaSynchro(
		std::shared_ptr<ISynchroObserver> observer = nullptr);
	~MediaSynchro();
	/* configs functions. */
	void	update(void* config)					 override;
	void	config(void* config)					 override;
	STATUS	status(void)							 override;
	int32_t	Q_size(void)							 override;
	void	updateSyncType(int32_t sync_type)		 override;
	void    onMFrm(std::shared_ptr<MRframe> avfrm)	 override;
	/* control functions. */
	void start(void)								 override;
	void stopd(bool stop_quik = false)				 override;
	void pause(bool pauseflag = false)				 override;
private:
	double decodeClock();
	double getSyncAdjustedPtsDiff(double pts, double pts_diff);
private:
	std::weak_ptr<ISynchroObserver>		m_observer;
	std::atomic<STATUS>					m_status{ E_INVALID };
	
	std::mutex							m_cmutex;
	std::shared_ptr<MSynConfig>			m_config; //配置参数

	std::atomic<bool>					m_signalAquit{ true };
	std::atomic<bool>					m_signalArset{ true };
	std::shared_ptr<MRframe>			m_acache{ nullptr };	
	std::thread 						m_asychro_worker;
	AT::SafeQueue<std::shared_ptr<MRframe>> m_asychro_Q;

	std::atomic<bool>					m_signalVrset{ true };
	std::atomic<bool>					m_signalVquit{ true };
	std::shared_ptr<MRframe>			m_vcache{ nullptr };
	std::thread 						m_vsychro_worker;
	AT::SafeQueue<std::shared_ptr<MRframe>> m_vsychro_Q;

	double m_current_ptsv = 0;
	int64_t m_current_pts_timev = 0;
	double m_previous_ptsv = 0;
	double m_previous_pts_diffv = 40e-3;
	int64_t m_start_ptsv = 0;
	double m_predicted_ptsv = 0.0;
	bool m_first_framev = true;
	double m_next_wakev = 0.0;

	double m_current_pts = 0;
	int64_t m_current_pts_time = 0;
	double m_previous_pts = 0;
	double m_previous_pts_diff = 40e-3;
	int64_t m_start_pts = 0;
	double m_predicted_pts = 0.0;
	bool m_first_frame = true;
	double m_next_wake = 0.0;
};
