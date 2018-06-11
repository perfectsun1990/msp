
#pragma  once

#include "pubcore.hpp"
#include "demuxer.hpp"
#include "decoder.hpp"
#include "synchro.hpp"
#include "mrender.hpp"

class Mplayer :
	public std::enable_shared_from_this<Mplayer>,
	public IDemuxerObserver,
	public IDecoderObserver,
	public ISynchroObserver,
	public IMrenderObserver
{
public:
	Mplayer(const char* url, 
		const char* speakr = "", const void* window = nullptr);
	~Mplayer();
	/* configs functions. */
	STATUS	status(void);					 
	void	config(void* config);					 
	void	update(void* config);					 
	int64_t durats(void);//ms
	/* control functions. */
	bool	mInit(void);
	void	start(void);
	void	stopd(void);
	void	pause(bool pauseflag);
	void	seekp(int64_t seektp);
private:
	// Demuxer->Decoder
	void onMPkt(std::shared_ptr<MPacket> av_pkt)	  override;
	// Decoder->Synchro								  
	void onMFrm(std::shared_ptr<MRframe> av_frm)	  override;
	// Synchro->Mrender								  
	void onSync(std::shared_ptr<MRframe> av_frm)	  override;
	// Mrender->Maneger.							  
	void onMPts(int32_t type, double upts)			  override;
private:
	std::atomic<STATUS>					m_status{ E_INVALID };
	std::atomic<bool>					m_signal_quit{ false };
	std::atomic<bool>					m_signal_init{ false };
	std::atomic<bool>					m_signal_rest{ false };

	std::string							m_inputf{ "" };
	std::string							m_speakr{ "" };
	const void*							m_window{ nullptr };
	//
	std::shared_ptr<ISynchro> 			m_msynchro{ nullptr };
	std::shared_ptr<IDemuxer> 			m_mdemuxer{ nullptr };
	std::shared_ptr<IDecoder> 			m_adecoder{ nullptr };
	std::shared_ptr<IDecoder> 			m_vdecoder{ nullptr };
	std::shared_ptr<IMrender> 			m_amrender{ nullptr };
	std::shared_ptr<IMrender> 			m_vmrender{ nullptr };

};