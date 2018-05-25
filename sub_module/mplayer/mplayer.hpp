
#pragma  once

#include "pubcore.hpp"
#include "demuxer.hpp"
#include "decoder.hpp"
#include "mrender.hpp"

class IMpTimerObserver
{
public:
	IMpTimerObserver();
	~IMpTimerObserver();

private:

};

#define AV_SYNC_THRESHOLD 0.01
#define AV_NOSYNC_THRESHOLD 10.0

class Clock
{
public:
	Clock();
	~Clock();

private:

};

class Mplayer :
	public std::enable_shared_from_this<Mplayer>,
	public IDemuxerObserver,
	public IDecoderObserver,
	public IMrenderObserver
{
public:
	Mplayer(const char* url, const char* speakr = "", const void* window = nullptr);
	~Mplayer();
	// Demuxer->Decoder
	void onPacket(std::shared_ptr<MPacket> av_pkt)	override;
	// Decoder->Mrender
	void onMFrame(std::shared_ptr<MRframe> av_frm)	override;
	// Mrender->Maneger.
	void onMPoint(int32_t type, double upts)		override;
	// Mplayer start & stop.
	void start();
	void stopd();
private:
	double decodeClock();
	double getSyncAdjustedPtsDiff(double pts, double pts_diff);
private:
	std::atomic<bool>				m_sig_quit{ false };
	std::atomic<bool>				m_init_flag{ false };
	const char*						m_inputf{ nullptr };
	const char*						m_speakr{ nullptr };
	const void*						m_window{ nullptr };
	std::shared_ptr<IDemuxer> 		m_mdemuxer{ nullptr };
	std::shared_ptr<IDecoder> 		m_adecoder{ nullptr };
	std::shared_ptr<IDecoder> 		m_vdecoder{ nullptr };
	std::shared_ptr<IMrender> 		m_amrender{ nullptr };
	std::shared_ptr<IMrender> 		m_vmrender{ nullptr };
	std::queue<std::shared_ptr<MRframe>> m_vrender_Q;
	std::mutex						m_vrender_Q_mutx;
	std::condition_variable			m_vrender_Q_cond;
	std::queue<std::shared_ptr<MRframe>> m_arender_Q;
	std::mutex						m_arender_Q_mutx;
	std::condition_variable			m_arender_Q_cond;
	std::thread						m_vrefesh_worker;
	std::thread						m_arefesh_worker;

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