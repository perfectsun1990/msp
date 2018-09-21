
#pragma  once

#include "pubcore.hpp"
#include "demuxer.hpp"
//#include "enmuxer.hpp"
#include "decoder.hpp"
#include "encoder.hpp"
#include "synchro.hpp"
#include "mrender.hpp"

/////////////////////////////////////////////////////////////////////////////////////
class IMplayerObserver
{// default use ffmpeg.
public:
	virtual void onCurRenderPts(int32_t ssid, int32_t type, double upts) = 0;
// 	virtual void onMplayerStart(void) = 0;
// 	virtual void onMplayerStopd(void) = 0;
// 	virtual void onMplayerPause(void) = 0;
// 	virtual void onMplayerSeekp(void) = 0;
};

class IMplayer
{
public:
	static std::shared_ptr<IMplayer> create(const char* url, const char* speakr = "", const void* window = nullptr,
		std::shared_ptr<IMplayerObserver> m_observer = nullptr);
	/* configs functions. */
	virtual int32_t	ssidNo(void) = 0;
	virtual void	update(void* config) = 0;
	virtual void	config(void* config) = 0;
	virtual STATUS	status(void) = 0;
	virtual double  durats(void) = 0;
	/* control functions. */
	virtual void	start(void) = 0;
	virtual void	stopd(void) = 0;
	virtual void	pause(bool pauseflag = false) = 0;
	virtual void	seekp(int64_t seektp) = 0;
protected:
	virtual ~IMplayer() = default;
};
/////////////////////////////////////////////////////////////////////////////////////
struct MplyerCfg
{
	struct MplyerPars {
		double								duts_time{ 0 }; //duration/s.
		double								curr_time{ 0 }; //demuxer not render ts/s.
		int64_t								strm_nums;
	};
	std::string								inputpath{ "" };
	std::string								rd_speakr{ "" };
	const void*								rd_window{ nullptr };
	bool									pauseflag{ false };
	int64_t									seek_time{ 0 };	//ms.
};
class Mplayer :
	public std::enable_shared_from_this<Mplayer>,
	public IDemuxerObserver,
	//public IEnmuxerObserver,
	public IDecoderObserver,
	public IEncoderObserver,
	public ISynchroObserver,
	public IMrenderObserver,
	public IMplayer
{
public:
	Mplayer(const char* inputf, const char* speakr = "", const void* window = nullptr,
		std::shared_ptr<IMplayerObserver> m_observer = nullptr);
	~Mplayer();
	/* configs functions. */
	int32_t	ssidNo(void)									override;
	void	update(void* config)							override;
	void	config(void* config)							override;
	STATUS	status(void)									override;
	double  durats(void)									override;//ms
	/* control functions. */
	bool	mInit(void);
	void	start(void)										override;
	void	stopd(void)										override;
	void	pause(bool pauseflag = false)					override;
	void	seekp(int64_t seektp)							override;
private:
	void updateAttributes(void);
	// Demuxer->Decoder
	void onDemuxerPackt(std::shared_ptr<MPacket> av_pkt)	override;
	// Decoder->Synchro								  
	void onDecoderFrame(std::shared_ptr<MRframe> av_frm)	override;
	// Synchro->Mrender								  
	void onSynchroFrame(std::shared_ptr<MRframe> av_frm)	override;
	// Mrender->Encoder.							  
	void onMRenderFrame(std::shared_ptr<MRframe> av_frm)	override;
	// Encoder->Enmuxer.
	void onEncoderPackt(std::shared_ptr<MPacket> av_pkt)	override;
	//void onDecoderFrame(std::shared_ptr<MRframe> av_frm)	override;
private:
	std::weak_ptr<IMplayerObserver>			  m_observer;
	std::atomic<STATUS>						  m_status{ E_INVALID };
	std::atomic<bool>						  m_signal_quit{ false };
	std::atomic<bool>						  m_signal_init{ false };
	std::atomic<bool>						  m_signal_rest{ false };
											  
	std::mutex								  m_cmutex;
	std::shared_ptr<MplyerCfg>				  m_config;
	int32_t									  m_ssidNo{ -1 };

	std::shared_ptr<IDecoder> 				  m_adecoder1{ nullptr };
	std::shared_ptr<IDecoder> 				  m_vdecoder1{ nullptr };

	//										  
	std::shared_ptr<ISynchro> 				  m_msynchro{ nullptr };
	std::shared_ptr<IDemuxer> 				  m_mdemuxer{ nullptr };
	std::shared_ptr<IDecoder> 				  m_adecoder{ nullptr };
	std::shared_ptr<IDecoder> 				  m_vdecoder{ nullptr };
	std::shared_ptr<IEncoder> 				  m_aencoder{ nullptr };
	std::shared_ptr<IEncoder> 				  m_vencoder{ nullptr };
	std::shared_ptr<IMrender> 				  m_amrender{ nullptr };
	std::shared_ptr<IMrender> 				  m_vmrender{ nullptr };

};