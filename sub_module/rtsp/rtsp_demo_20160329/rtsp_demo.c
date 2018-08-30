/*************************************************************************
	> File Name: rtsp_demo.c
	> Author: bxq
	> Mail: 544177215@qq.com 
	> Created Time: Monday, November 23, 2015 AM12:34:09 CST
 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>


#include "comm.h"
#include "rtsp_demo.h"
#include "rtsp_msg.h"
#include "rtp_enc.h"
#include "queue.h"

//TODO LIST
//FIX RTP/RTCP port cross NAP server on WAN
//RTCP support
//SDP H264 SPS/PPS

struct rtsp_session;
struct rtsp_client_connection;
TAILQ_HEAD(rtsp_session_queue_head, rtsp_session);
TAILQ_HEAD(rtsp_client_connection_queue_head, rtsp_client_connection);

struct rtsp_session
{
	char path[64];
	int  has_video;
	int  has_audio;
	uint8_t h264_sps[64];
	uint8_t h264_pps[64];
	int     h264_sps_len;
	int     h264_pps_len;

	rtp_enc vrtpe;
	rtp_enc artpe;
	
	struct rtsp_demo *demo;
	struct rtsp_client_connection_queue_head connections_qhead;
	TAILQ_ENTRY(rtsp_session) demo_entry;
};

struct rtp_connection
{
	int is_over_tcp;
	int tcp_sockfd; //if is_over_tcp=1. rtsp socket
	int tcp_interleaved;//if is_over_tcp=1. is rtp interleaved, rtcp interleaved = rtp interleaved + 1
	int udp_sockfd[2]; //if is_over_tcp=0. [0] is rtp socket, [1] is rtcp socket
	uint32_t ssrc;
};

struct rtsp_client_connection
{
	int state;	//session state
#define RTSP_CC_STATE_INIT		0
#define RTSP_CC_STATE_READY		1
#define RTSP_CC_STATE_PLAYING	2
#define RTSP_CC_STATE_RECORDING	3

	int sockfd;		//rtsp client socket
	unsigned long session_id;	//session id

	char reqbuf[1024];
	int  reqlen;

	struct rtp_connection *vrtp;
	struct rtp_connection *artp;

	struct rtsp_demo *demo;
	struct rtsp_session *session;
	TAILQ_ENTRY(rtsp_client_connection) demo_entry;	
	TAILQ_ENTRY(rtsp_client_connection) session_entry;
};

struct rtsp_demo 
{
	int  sockfd;	//rtsp server socket 0:invalid
	struct rtsp_session_queue_head sessions_qhead;
	struct rtsp_client_connection_queue_head connections_qhead;
};

static struct rtsp_demo *__alloc_demo (void)
{
	struct rtsp_demo *d = (struct rtsp_demo*) calloc(1, sizeof(struct rtsp_demo));
	if (NULL == d) {
		err("alloc memory for rtsp_demo failed\n");
		return NULL;
	}
	TAILQ_INIT(&d->sessions_qhead);
	TAILQ_INIT(&d->connections_qhead);
	return d;
}

static void __free_demo (struct rtsp_demo *d)
{
	if (d) {
		free(d);
	}
}

static struct rtsp_session *__alloc_session (struct rtsp_demo *d)
{
	struct rtsp_session *s = (struct rtsp_session*) calloc(1, sizeof(struct rtsp_session));
	if (NULL == s) {
		err("alloc memory for rtsp_session failed\n");
		return NULL;
	}

	s->demo = d;
	TAILQ_INIT(&s->connections_qhead);
	TAILQ_INSERT_TAIL(&d->sessions_qhead, s, demo_entry);
	return s;
}

static void __free_session (struct rtsp_session *s)
{
	if (s) {
		struct rtsp_demo *d = s->demo; 
		TAILQ_REMOVE(&d->sessions_qhead, s, demo_entry);
		free(s);
	}
}

static struct rtsp_client_connection *__alloc_client_connection (struct rtsp_demo *d)
{
	struct rtsp_client_connection *cc = (struct rtsp_client_connection*) calloc(1, sizeof(struct rtsp_client_connection));
	if (NULL == cc) {
		err("alloc memory for rtsp_session failed\n");
		return NULL;
	}

	cc->demo = d;
	TAILQ_INSERT_TAIL(&d->connections_qhead, cc, demo_entry);
	return cc;
}

static void __free_client_connection (struct rtsp_client_connection *cc)
{
	if (cc) {
		struct rtsp_demo *d = cc->demo;
		TAILQ_REMOVE(&d->connections_qhead, cc, demo_entry);
		free(cc);
	}
}

static void __client_connection_bind_session (struct rtsp_client_connection *cc, struct rtsp_session *s)
{
	if (cc->session == NULL) {
		cc->session = s;
		TAILQ_INSERT_TAIL(&s->connections_qhead, cc, session_entry);
	}
}

static void __client_connection_unbind_session (struct rtsp_client_connection *cc)
{
	struct rtsp_session *s = cc->session;
	if (s) {
		TAILQ_REMOVE(&s->connections_qhead, cc, session_entry);
		cc->session = NULL;
	}
}

/*****************************************************************************
* b64_encode: Stolen from VLC's http.c.
* Simplified by Michael.
* Fixed edge cases and made it work from data (vs. strings) by Ryan.
*****************************************************************************/
static char *base64_encode (char *out, int out_size, const uint8_t *in, int in_size)
{
    static const char b64[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    char *ret, *dst;
    unsigned i_bits = 0;
    int i_shift = 0;
    int bytes_remaining = in_size;

#define __UINT_MAX (~0lu)
#define __BASE64_SIZE(x)  (((x)+2) / 3 * 4 + 1)
#define __RB32(x)                                \
    (((uint32_t)((const uint8_t*)(x))[0] << 24) |    \
               (((const uint8_t*)(x))[1] << 16) |    \
               (((const uint8_t*)(x))[2] <<  8) |    \
                ((const uint8_t*)(x))[3])
    if (in_size >= __UINT_MAX / 4 ||
        out_size < __BASE64_SIZE(in_size))
        return NULL;
    ret = dst = out;
    while (bytes_remaining > 3) {
        i_bits = __RB32(in);
        in += 3; bytes_remaining -= 3;
        *dst++ = b64[ i_bits>>26        ];
        *dst++ = b64[(i_bits>>20) & 0x3F];
        *dst++ = b64[(i_bits>>14) & 0x3F];
        *dst++ = b64[(i_bits>>8 ) & 0x3F];
    }
    i_bits = 0;
    while (bytes_remaining) {
        i_bits = (i_bits << 8) + *in++;
        bytes_remaining--;
        i_shift += 8;
    }
    while (i_shift > 0) {
        *dst++ = b64[(i_bits << 6 >> i_shift) & 0x3f];
        i_shift -= 6;
    }
    while ((dst - ret) & 3)
        *dst++ = '=';
    *dst = '\0';

    return ret;
}

static int build_simple_sdp (char *sdpbuf, int maxlen, const char *uri, int has_video, int has_audio,
	const uint8_t *sps, int sps_len, const uint8_t *pps, int pps_len)
{
	char *p = sdpbuf;

	p += sprintf(p, "v=0\r\n");
	p += sprintf(p, "o=- 0 0 IN IP4 0.0.0.0\r\n");
	p += sprintf(p, "s=h264+pcm_alaw\r\n");
	p += sprintf(p, "t=0 0\r\n");
	p += sprintf(p, "a=control:%s\r\n", uri ? uri : "*");
	p += sprintf(p, "a=range:npt=0-\r\n");

	if (has_video) {
		p += sprintf(p, "m=video 0 RTP/AVP 96\r\n");
		p += sprintf(p, "c=IN IP4 0.0.0.0\r\n");
		p += sprintf(p, "a=rtpmap:96 H264/90000\r\n");
		if (sps && pps && sps_len > 0 && pps_len > 0) {
			if (sps[0] == 0 && sps[1] == 0 && sps[2] == 1) {
				sps += 3;
				sps_len -= 3;
			}
			if (sps[0] == 0 && sps[1] == 0 && sps[2] == 0 && sps[3] == 1) {
				sps += 4;
				sps_len -= 4;
			}
			if (pps[0] == 0 && pps[1] == 0 && pps[2] == 1) {
				pps += 3;
				pps_len -= 3;
			}
			if (pps[0] == 0 && pps[1] == 0 && pps[2] == 0 && pps[3] == 1) {
				pps += 4;
				pps_len -= 4;
			}
			p += sprintf(p, "a=fmtp:96 packetization-mode=1;sprop-parameter-sets=");
			base64_encode(p, (maxlen - (p - sdpbuf)), sps, sps_len);
			p += strlen(p);
			p += sprintf(p, ",");
			base64_encode(p, (maxlen - (p - sdpbuf)), pps, pps_len);
			p += strlen(p);
			p += sprintf(p, "\r\n");
		} else {
			p += sprintf(p, "a=fmtp:96 packetization-mode=1\r\n");
		}
		if (uri)
			p += sprintf(p, "a=control:%s/track1\r\n", uri);
		else
			p += sprintf(p, "a=control:track1\r\n");
	}

	if (has_audio) {
#if 0
		p += sprintf(p, "m=audio 0 RTP/AVP 8\r\n"); //PCMA/8000/1 (G711A)
#else
		p += sprintf(p, "m=audio 0 RTP/AVP 97\r\n");
		p += sprintf(p, "c=IN IP4 0.0.0.0\r\n");
		p += sprintf(p, "a=rtpmap:97 PCMA/8000/1\r\n");
#endif
		if (uri)
			p += sprintf(p, "a=control:%s/track2\r\n", uri);
		else
			p += sprintf(p, "a=control:track2\r\n");
	}

	return (p - sdpbuf);
}

rtsp_demo_handle rtsp_new_demo (int port)
{
	struct rtsp_demo *d = NULL;
	struct sockaddr_in inaddr;
	int sockfd, ret;
	
#ifdef __WIN32__
	WSADATA ws;
	WSAStartup(MAKEWORD(2,2), &ws);
#endif

	d = __alloc_demo();
	if (NULL == d) {
		return NULL;
	}

	ret = socket(AF_INET, SOCK_STREAM, 0);
	if (ret < 0) {
		err("create socket failed : %s\n", strerror(errno));
		__free_demo(d);
		return NULL;
	}
	sockfd = ret;

	if (port <= 0)
		port = 554;

	memset(&inaddr, 0, sizeof(inaddr));
	inaddr.sin_family = AF_INET;
	inaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	inaddr.sin_port = htons(port);
	ret = bind(sockfd, (struct sockaddr*)&inaddr, sizeof(inaddr));
	if (ret < 0) {
		err("bind socket to address failed : %s\n", strerror(errno));
		closesocket(sockfd);
		__free_demo(d);
		return NULL;
	}

	ret = listen(sockfd, 100); //XXX
	if (ret < 0) {
		err("listen socket failed : %s\n", strerror(errno));
		closesocket(sockfd);
		__free_demo(d);
		return NULL;
	}

	d->sockfd = sockfd;

	info("rtsp server demo starting on port %d\n", port);
	return (rtsp_demo_handle)d;
}

#ifdef __WIN32__
#include <mstcpip.h>
#endif
#ifdef __LINUX__
#include <fcntl.h>
#include <netinet/tcp.h>
#endif

static int rtsp_set_client_socket (int sockfd)
{
		int ret;

#ifdef __WIN32__
		unsigned long nonblocked = 1;
		int sndbufsiz = 1024 * 512;
		int keepalive = 1;
		struct tcp_keepalive alive_in, alive_out;
		unsigned long alive_retlen;
		struct linger ling;

		ret = ioctlsocket(sockfd, FIONBIO, &nonblocked);
		if (ret < 0) {
			warn("ioctlsocket FIONBIO failed\n");
		}

		ret = setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, (const char*)&sndbufsiz, sizeof(sndbufsiz));
		if (ret < 0) {
			warn("setsockopt SO_SNDBUF failed\n");
		}

		ret = setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, (const char*)&keepalive, sizeof(keepalive));
		if (ret < 0) {
			warn("setsockopt setsockopt SO_KEEPALIVE failed\n");
		}

		alive_in.onoff = TRUE;
		alive_in.keepalivetime = 60000;
		alive_in.keepaliveinterval = 30000;
		ret = WSAIoctl(sockfd, SIO_KEEPALIVE_VALS, &alive_in, sizeof(alive_in), 
			&alive_out, sizeof(alive_out), &alive_retlen, NULL, NULL);
		if (ret == SOCKET_ERROR) {
			warn("WSAIoctl SIO_KEEPALIVE_VALS failed\n");
		}

		memset(&ling, 0, sizeof(ling));
		ling.l_onoff = 1;
		ling.l_linger = 0;
		ret = setsockopt(sockfd, SOL_SOCKET, SO_LINGER, (const char*)&ling, sizeof(ling)); //resolve too many TCP CLOSE_WAIT
		if (ret < 0) {
			warn("setsockopt SO_LINGER failed\n");
		}
#endif

#ifdef __LINUX__
		int sndbufsiz = 1024 * 512;
		int keepalive = 1;
		int keepidle = 60;
		int keepinterval = 3;
		int keepcount = 5;
		struct linger ling;

		ret = fcntl(sockfd, F_GETFL, 0);
		if (ret < 0) {
			warn("fcntl F_GETFL failed\n");
		} else {
			ret |= O_NONBLOCK;
			ret = fcntl(sockfd, F_SETFL, ret);
			if (ret < 0) {
				warn("fcntl F_SETFL failed\n");
			}
		}

		ret = setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, (const char*)&sndbufsiz, sizeof(sndbufsiz));
		if (ret < 0) {
			warn("setsockopt SO_SNDBUF failed\n");
		}

		ret = setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, (const char*)&keepalive, sizeof(keepalive));
		if (ret < 0) {
			warn("setsockopt SO_KEEPALIVE failed\n");
		}

		ret = setsockopt(sockfd, SOL_TCP, TCP_KEEPIDLE, (const char*)&keepidle, sizeof(keepidle));
		if (ret < 0) {
			warn("setsockopt TCP_KEEPIDLE failed\n");
		}

		ret = setsockopt(sockfd, SOL_TCP, TCP_KEEPINTVL, (const char*)&keepinterval, sizeof(keepinterval));
		if (ret < 0) {
			warn("setsockopt TCP_KEEPINTVL failed\n");
		}

		ret = setsockopt(sockfd, SOL_TCP, TCP_KEEPCNT, (const char*)&keepcount, sizeof(keepcount));
		if (ret < 0) {
			warn("setsockopt TCP_KEEPCNT failed\n");
		}

		memset(&ling, 0, sizeof(ling));
		ling.l_onoff = 1;
		ling.l_linger = 0;
		ret = setsockopt(sockfd, SOL_SOCKET, SO_LINGER, (const char*)&ling, sizeof(ling)); //resolve too many TCP CLOSE_WAIT
		if (ret < 0) {
			warn("setsockopt SO_LINGER failed\n");
		}
#endif
		return 0;
}

static struct rtsp_client_connection *rtsp_new_client_connection (struct rtsp_demo *d)
{
	struct rtsp_client_connection *cc = NULL;
	struct sockaddr_in inaddr;
	int sockfd;
	socklen_t addrlen = sizeof(inaddr);

	int ret = accept(d->sockfd, (struct sockaddr*)&inaddr, &addrlen);
	if (ret < 0) {
		err("accept failed : %s\n", strerror(errno));
		return NULL;
	}
	sockfd = ret;
	rtsp_set_client_socket(sockfd);//XXX DEBUG

	info("new rtsp client %s:%u comming\n", 
			inet_ntoa(inaddr.sin_addr), ntohs(inaddr.sin_port));

	cc = __alloc_client_connection(d);
	if (cc == NULL) {
		closesocket(sockfd);
		return NULL;
	}

	cc->state = RTSP_CC_STATE_INIT;
	cc->sockfd = sockfd;

	return cc;
}

static void rtsp_del_rtp_connection(struct rtsp_client_connection *cc, int isaudio);
static void rtsp_del_client_connection (struct rtsp_client_connection *cc)
{
	if (cc) {
		info("delete client %d\n", cc->sockfd);
		__client_connection_unbind_session(cc);
		rtsp_del_rtp_connection(cc, 0);
		rtsp_del_rtp_connection(cc, 1);
		closesocket(cc->sockfd);
		__free_client_connection(cc);
	}
}

rtsp_session_handle rtsp_new_session (rtsp_demo_handle demo, const char *path, int has_video, int has_audio)
{
	struct rtsp_demo *d = (struct rtsp_demo*)demo;
	struct rtsp_session *s = NULL;

	if (!d || !path || strlen(path) == 0 || (has_video == 0 && has_audio == 0)) {
		err("param invalid\n");
		return NULL;
	}

	TAILQ_FOREACH(s, &d->sessions_qhead, demo_entry) {
		if (strstr(path, s->path) || strstr(s->path, path)) {
			err("has likely path:%s (%s)\n", s->path, path);
			return NULL;
		}
	}

	s = __alloc_session(d);
	if (NULL == s) {
		return NULL;
	}

	strncpy(s->path, path, sizeof(s->path) - 1);
	s->has_video = !!has_video;
	s->has_audio = !!has_audio;

#define RTP_MAX_PKTSIZ	((1500-42)/4*4)//不超过1460bytes大小
#define RTP_MAX_NBPKTS	(300)
	if (s->has_video) {
		s->vrtpe.ssrc = 0;
		s->vrtpe.seq = 0;
		s->vrtpe.pt = 96;
		s->vrtpe.sample_rate = 90000;
		rtp_enc_init(&s->vrtpe, RTP_MAX_PKTSIZ, RTP_MAX_NBPKTS);
	}
	if (s->has_audio) {
		s->artpe.ssrc = 0;
		s->artpe.seq = 0;
#if 1
		s->artpe.pt = 97;
#else
		s->artpe.pt = 8;
#endif
		s->artpe.sample_rate = 8000;
		rtp_enc_init(&s->artpe, RTP_MAX_PKTSIZ, 5);
	}

	info("add session path: %s\n", s->path);
	return (rtsp_session_handle)s;
}

void rtsp_del_session (rtsp_session_handle session)
{
	struct rtsp_session *s = (struct rtsp_session*)session;
	if (s) {
		struct rtsp_client_connection *cc;
		while ((cc = TAILQ_FIRST(&s->connections_qhead))) {
			rtsp_del_client_connection(cc);
		}
		info("del session path: %s\n", s->path);
		if (s->has_video) {
			rtp_enc_deinit(&s->vrtpe);
		}
		if (s->has_audio) {
			rtp_enc_deinit(&s->artpe);
		}
		__free_session(s);
	}
}

void rtsp_del_demo (rtsp_demo_handle demo)
{
	struct rtsp_demo *d = (struct rtsp_demo*)demo;
	if (d) {
		struct rtsp_session *s;
		struct rtsp_client_connection *cc;
		
		while ((cc = TAILQ_FIRST(&d->connections_qhead))) {
			rtsp_del_client_connection(cc);
		}
		while ((s = TAILQ_FIRST(&d->sessions_qhead))) {
			rtsp_del_session(s);
		}

		closesocket(d->sockfd);
		__free_demo(d);
	}
}

static int rtsp_handle_OPTIONS(struct rtsp_client_connection *cc, const rtsp_msg_s *reqmsg, rtsp_msg_s *resmsg)
{
//	struct rtsp_demo *d = cc->demo;
//	struct rtsp_session *s = cc->session;
	uint32_t public_ = 0;
	dbg("\n");
	public_ |= RTSP_MSG_PUBLIC_OPTIONS;
	public_ |= RTSP_MSG_PUBLIC_DESCRIBE;
	public_ |= RTSP_MSG_PUBLIC_SETUP;
	public_ |= RTSP_MSG_PUBLIC_PAUSE;
	public_ |= RTSP_MSG_PUBLIC_PLAY;
	public_ |= RTSP_MSG_PUBLIC_TEARDOWN;
	rtsp_msg_set_public(resmsg, public_);
	return 0;
}

static int rtsp_handle_DESCRIBE(struct rtsp_client_connection *cc, const rtsp_msg_s *reqmsg, rtsp_msg_s *resmsg)
{
//	struct rtsp_demo *d = cc->demo;
	struct rtsp_session *s = cc->session;
	char sdp[512] = "";
	int len = 0;
	uint32_t accept = 0;
	const rtsp_msg_uri_s *puri = &reqmsg->hdrs.startline.reqline.uri;
	char uri[128] = "";

	dbg("\n");
	if (rtsp_msg_get_accept(reqmsg, &accept) < 0 && !(accept & RTSP_MSG_ACCEPT_SDP)) {
		rtsp_msg_set_response(resmsg, 406);
		warn("client not support accept SDP\n");
		return 0;
	}

	//build uri
	if (puri->scheme == RTSP_MSG_URI_SCHEME_RTSPU)
		strcat(uri, "rtspu://");
	else
		strcat(uri, "rtsp://");
	strcat(uri, puri->ipaddr);
	if (puri->port != 0)
		sprintf(uri + strlen(uri), ":%u", puri->port);
	strcat(uri, s->path);
	
	len = build_simple_sdp(sdp, sizeof(sdp), uri, s->has_video, s->has_audio, 
		s->h264_sps, s->h264_sps_len, s->h264_pps, s->h264_pps_len);

	rtsp_msg_set_content_type(resmsg, RTSP_MSG_CONTENT_TYPE_SDP);
	rtsp_msg_set_content_length(resmsg, len);
	resmsg->body.body = rtsp_mem_dup(sdp, len);
	return 0;
}

static unsigned long __rtp_gen_ssrc(void)
{
	static unsigned long ssrc = 0x22345678;
	return ssrc++;
}

static int __rtp_rtcp_socket(int *rtpsock, int *rtcpsock, const char *peer_ip, int peer_port)
{
	int i, ret;

	for (i = 65536/4*3/2*2; i < 65536; i += 2) {
		struct sockaddr_in inaddr;
		uint16_t port;

		*rtpsock = socket(AF_INET, SOCK_DGRAM, 0);
		if (*rtpsock < 0) {
			err("create rtp socket failed: %s\n", strerror(errno));
			return -1;
		}

		*rtcpsock = socket(AF_INET, SOCK_DGRAM, 0);
		if (*rtcpsock < 0) {
			err("create rtcp socket failed: %s\n", strerror(errno));
			closesocket(*rtpsock);
			return -1;
		}

		port = i;
		memset(&inaddr, 0, sizeof(inaddr));
		inaddr.sin_family = AF_INET;
		inaddr.sin_addr.s_addr = htonl(INADDR_ANY);
		inaddr.sin_port = htons(port);
		ret = bind(*rtpsock, (struct sockaddr*)&inaddr, sizeof(inaddr));
		if (ret < 0) {
			closesocket(*rtpsock);
			closesocket(*rtcpsock);
			continue;
		}

		port = i + 1;
		memset(&inaddr, 0, sizeof(inaddr));
		inaddr.sin_family = AF_INET;
		inaddr.sin_addr.s_addr = htonl(INADDR_ANY);
		inaddr.sin_port = htons(port);
		ret = bind(*rtcpsock, (struct sockaddr*)&inaddr, sizeof(inaddr));
		if (ret < 0) {
			closesocket(*rtpsock);
			closesocket(*rtcpsock);
			continue;
		}

		port = peer_port / 2 * 2;
		memset(&inaddr, 0, sizeof(inaddr));
		inaddr.sin_family = AF_INET;
		inaddr.sin_addr.s_addr = inet_addr(peer_ip);
		inaddr.sin_port = htons(port);
		ret = connect(*rtpsock, (struct sockaddr*)&inaddr, sizeof(inaddr));
		if (ret < 0) {
			closesocket(*rtpsock);
			closesocket(*rtcpsock);
			err("connect peer rtp port failed: %s\n", strerror(errno));
			return -1;
		}

		port = peer_port / 2 * 2 + 1;
		memset(&inaddr, 0, sizeof(inaddr));
		inaddr.sin_family = AF_INET;
		inaddr.sin_addr.s_addr = inet_addr(peer_ip);
		inaddr.sin_port = htons(port);
		ret = connect(*rtcpsock, (struct sockaddr*)&inaddr, sizeof(inaddr));
		if (ret < 0) {
			closesocket(*rtpsock);
			closesocket(*rtcpsock);
			err("connect peer rtcp port failed: %s\n", strerror(errno));
			return -1;
		}

		return i;
	}

	err("not found free local port for rtp/rtcp\n");
	return -1;
}

static int rtsp_new_rtp_connection(struct rtsp_client_connection *cc, const char *peer_ip, int peer_port_interleaved, int isaudio, int istcp)
{
	struct rtp_connection *rtp;
	int ret;

	rtp = (struct rtp_connection*) calloc(1, sizeof(struct rtp_connection));
	if (rtp == NULL) {
		err("alloc mem for rtp session failed: %s\n", strerror(errno));
		return -1;
	}

	rtp->is_over_tcp = istcp;
	rtp->ssrc = __rtp_gen_ssrc();

	if (istcp) {
		rtp->tcp_sockfd = cc->sockfd;
		rtp->tcp_interleaved = peer_port_interleaved;
		ret = rtp->tcp_interleaved;
	} else {
		int rtpsock, rtcpsock;
		ret = __rtp_rtcp_socket(&rtpsock, &rtcpsock, peer_ip, peer_port_interleaved);
		if (ret < 0) {
			free(rtp);
			return -1;
		}

		rtp->udp_sockfd[0] = rtpsock;
		rtp->udp_sockfd[1] = rtcpsock;
	}

	if (isaudio)
		cc->artp = rtp;
	else
		cc->vrtp = rtp;
	info("rtsp session %08lX new %s %d-%d %s\n", 
			cc->session_id,
			(isaudio ? "artp" : "vrtp"), 
			ret, ret + 1, 
			(istcp ? "OverTCP" : "OverUDP"));
	return ret;
}

static void rtsp_del_rtp_connection(struct rtsp_client_connection *cc, int isaudio)
{
	struct rtp_connection *rtp;

	if (isaudio) {
		rtp = cc->artp;
		cc->artp = NULL;
	} else {
		rtp = cc->vrtp;
		cc->vrtp = NULL;
	}

	if (rtp) {
		if (!(rtp->is_over_tcp)) {
			closesocket(rtp->udp_sockfd[0]);
			closesocket(rtp->udp_sockfd[1]);
		}
		free(rtp);
	}
}

static const char *__get_peer_ip(int sockfd)
{
	struct sockaddr_in inaddr;
	socklen_t addrlen = sizeof(inaddr);
	int ret = getpeername(sockfd, (struct sockaddr*)&inaddr, &addrlen);
	if (ret < 0) {
		err("getpeername failed: %s\n", strerror(errno));
		return NULL;
	}
	return inet_ntoa(inaddr.sin_addr);
}

static int rtsp_handle_SETUP(struct rtsp_client_connection *cc, const rtsp_msg_s *reqmsg, rtsp_msg_s *resmsg)
{
//	struct rtsp_demo *d = cc->demo;
	struct rtsp_session *s = cc->session;
	uint32_t ssrc = 0;
	int istcp = 0, isaudio = 0;
	char vpath[64] = "";
	char apath[64] = "";

	dbg("\n");
	if (cc->state != RTSP_CC_STATE_INIT && cc->state != RTSP_CC_STATE_READY) 
	{
		rtsp_msg_set_response(resmsg, 455);
		warn("rtsp status err\n");
		return 0;
	}

	if (!reqmsg->hdrs.transport) {
		rtsp_msg_set_response(resmsg, 461);
		warn("rtsp no transport err\n");
		return 0;
	}

	if (reqmsg->hdrs.transport->type == RTSP_MSG_TRANSPORT_TYPE_RTP_AVP_TCP) {
		istcp = 1;
		if (!(reqmsg->hdrs.transport->flags & RTSP_MSG_TRANSPORT_FLAG_INTERLEAVED)) {
			warn("rtsp no interleaved err\n");
			rtsp_msg_set_response(resmsg, 461);
			return 0;
		}
	} else {
		if (!(reqmsg->hdrs.transport->flags & RTSP_MSG_TRANSPORT_FLAG_CLIENT_PORT)) {
			warn("rtsp no client_port err\n");
			rtsp_msg_set_response(resmsg, 461);
			return 0;
		}
	}

	snprintf(vpath, sizeof(vpath) - 1, "%s/track1", s->path);
	snprintf(apath, sizeof(vpath) - 1, "%s/track2", s->path);
	if (s->has_video && 0 == strncmp(reqmsg->hdrs.startline.reqline.uri.abspath, vpath, strlen(vpath))) {
		isaudio = 0;
	} else if (s->has_audio && 0 == strncmp(reqmsg->hdrs.startline.reqline.uri.abspath, apath, strlen(apath))) {
		isaudio = 1;
	} else {
		warn("rtsp urlpath:%s err\n", reqmsg->hdrs.startline.reqline.uri.abspath);
		rtsp_msg_set_response(resmsg, 461);
		return 0;
	}

	rtsp_del_rtp_connection(cc, isaudio);

	if (istcp) {
		int ret = rtsp_new_rtp_connection(cc, __get_peer_ip(cc->sockfd), 
			reqmsg->hdrs.transport->interleaved, isaudio, 1);
		if (ret < 0) {
			rtsp_msg_set_response(resmsg, 500);
			return 0;
		}
		ssrc = isaudio ? cc->artp->ssrc : cc->vrtp->ssrc;
		rtsp_msg_set_transport_tcp(resmsg, ssrc, 
				reqmsg->hdrs.transport->interleaved);
	} else {
		int ret = rtsp_new_rtp_connection(cc, __get_peer_ip(cc->sockfd), 
			reqmsg->hdrs.transport->client_port, isaudio, 0);
		if (ret < 0) {
			rtsp_msg_set_response(resmsg, 500);
			return 0;
		}
		ssrc = isaudio ? cc->artp->ssrc : cc->vrtp->ssrc;
		rtsp_msg_set_transport_udp(resmsg, ssrc, 
				reqmsg->hdrs.transport->client_port, ret);
	}

	if (cc->state == RTSP_CC_STATE_INIT) {
		cc->state = RTSP_CC_STATE_READY;
		cc->session_id = rtsp_msg_gen_session_id();
		rtsp_msg_set_session(resmsg, cc->session_id);
	}

	return 0;
}

static int rtsp_handle_PAUSE(struct rtsp_client_connection *cc, const rtsp_msg_s *reqmsg, rtsp_msg_s *resmsg)
{
//	struct rtsp_demo *d = cc->demo;
//	struct rtsp_session *s = cc->session;

	dbg("\n");
	if (cc->state != RTSP_CC_STATE_READY && cc->state != RTSP_CC_STATE_PLAYING) 
	{
		rtsp_msg_set_response(resmsg, 455);
		warn("rtsp status err\n");
		return 0;
	}

	if (cc->state != RTSP_CC_STATE_READY)
		cc->state = RTSP_CC_STATE_READY;
	return 0;
}

static int rtsp_handle_PLAY(struct rtsp_client_connection *cc, const rtsp_msg_s *reqmsg, rtsp_msg_s *resmsg)
{
//	struct rtsp_demo *d = cc->demo;
//	struct rtsp_session *s = cc->session;

	dbg("\n");
	if (cc->state != RTSP_CC_STATE_READY && cc->state != RTSP_CC_STATE_PLAYING) 
	{
		rtsp_msg_set_response(resmsg, 455);
		warn("rtsp status err\n");
		return 0;
	}

	if (cc->state != RTSP_CC_STATE_PLAYING)
		cc->state = RTSP_CC_STATE_PLAYING;
	return 0;
}

static int rtsp_handle_TEARDOWN(struct rtsp_client_connection *cc, const rtsp_msg_s *reqmsg, rtsp_msg_s *resmsg)
{
//	struct rtsp_demo *d = cc->demo;
//	struct rtsp_session *s = cc->session;

	dbg("\n");
	rtsp_del_rtp_connection(cc, 0);
	rtsp_del_rtp_connection(cc, 1);
	cc->state = RTSP_CC_STATE_INIT;
	return 0;
}

static int rtsp_process_request(struct rtsp_client_connection *cc, const rtsp_msg_s *reqmsg, rtsp_msg_s *resmsg)
{
	struct rtsp_demo *d = cc->demo;
	struct rtsp_session *s = cc->session;
	const char *path = reqmsg->hdrs.startline.reqline.uri.abspath;
	uint32_t cseq = 0, session = 0;
	rtsp_msg_set_response(resmsg, 200);

	if (s) {
		if (strncmp(path, s->path, strlen(s->path))) { //XXX rtsp://127.0.0.1/live/chn0000 rtsp://127.0.0.1/live/chn0 
			warn("Not allow modify path %s (old:%s)\n", path, s->path);
			rtsp_msg_set_response(resmsg, 451);
			return 0;
		}
	} else {
		TAILQ_FOREACH(s, &d->sessions_qhead, demo_entry) {
			if (0 == strncmp(path, s->path, strlen(s->path))) { //XXX rtsp://127.0.0.1/live/chn0000 rtsp://127.0.0.1/live/chn0 
				break;
			}
		}
		if (NULL == s) {
			warn("Not found session path: %s\n", path);
			rtsp_msg_set_response(resmsg, 454);
			return 0;
		}
		__client_connection_bind_session(cc, s);
	}

	if (rtsp_msg_get_cseq(reqmsg, &cseq) < 0) {
		rtsp_msg_set_response(resmsg, 400);
		warn("No CSeq field\n");
		return 0;
	}
	if (cc->state != RTSP_CC_STATE_INIT) {
		if (rtsp_msg_get_session(reqmsg, &session) < 0 || session != cc->session_id) {
			warn("Invalid Session field\n");
			rtsp_msg_set_response(resmsg, 454);
			return 0;
		}
	}

	rtsp_msg_set_cseq(resmsg, cseq);
	if (cc->state != RTSP_CC_STATE_INIT) {
		rtsp_msg_set_session(resmsg, session);
	}
	rtsp_msg_set_date(resmsg, NULL);
	rtsp_msg_set_server(resmsg, "rtsp_demo");

	switch (reqmsg->hdrs.startline.reqline.method) {
		case RTSP_MSG_METHOD_OPTIONS:
			return rtsp_handle_OPTIONS(cc, reqmsg, resmsg);
		case RTSP_MSG_METHOD_DESCRIBE:
			return rtsp_handle_DESCRIBE(cc, reqmsg, resmsg);
		case RTSP_MSG_METHOD_SETUP:
			return rtsp_handle_SETUP(cc, reqmsg, resmsg);
		case RTSP_MSG_METHOD_PAUSE:
			return rtsp_handle_PAUSE(cc, reqmsg, resmsg);
		case RTSP_MSG_METHOD_PLAY:
			return rtsp_handle_PLAY(cc, reqmsg, resmsg);
		case RTSP_MSG_METHOD_TEARDOWN:
			return rtsp_handle_TEARDOWN(cc, reqmsg, resmsg);
		default:
			break;
	}

	rtsp_msg_set_response(resmsg, 501);
	return 0;
}

static int rtsp_recv_msg(struct rtsp_client_connection *cc, rtsp_msg_s *msg)
{
	int ret;
	
	ret = recv(cc->sockfd, cc->reqbuf + cc->reqlen, sizeof(cc->reqbuf) - cc->reqlen - 1, MSG_DONTWAIT);
	if (ret <= 0) {
		err("recv data failed: %s\n", strerror(errno));
		return -1;
	}
	cc->reqlen += ret;
	cc->reqbuf[cc->reqlen] = 0;
	
	ret = rtsp_msg_parse_from_array(msg, cc->reqbuf, cc->reqlen);
	if (ret < 0) {
		err("Invalid frame\n");
		return -1;
	}
	if (ret == 0) {
		return 0;
	}

	memmove(cc->reqbuf, cc->reqbuf + ret, cc->reqlen - ret);
	cc->reqlen -= ret;
	return ret;
}

static int rtsp_send_msg(struct rtsp_client_connection *cc, rtsp_msg_s *msg) 
{
	char szbuf[1024] = "";
	int ret = rtsp_msg_build_to_array(msg, szbuf, sizeof(szbuf));
	if (ret < 0) {
		err("rtsp_msg_build_to_array failed\n");
		return -1;
	}

	ret = send(cc->sockfd, szbuf, ret, 0);
	if (ret < 0) {
		err("rtsp response send failed: %s\n", strerror(errno));
		return -1;
	}
	return ret;
}

int rtsp_do_event (rtsp_demo_handle demo)
{
	struct rtsp_demo *d = (struct rtsp_demo*)demo;
	struct rtsp_client_connection *cc = NULL;
	rtsp_msg_s reqmsg, resmsg;
	struct timeval tv;
	fd_set rfds;
	int maxfd, ret;

	if (NULL == d) {
		return -1;
	}

	FD_ZERO(&rfds);
	FD_SET(d->sockfd, &rfds);

	maxfd = d->sockfd;
	TAILQ_FOREACH(cc, &d->connections_qhead, demo_entry) 
	{
		FD_SET(cc->sockfd, &rfds);
		if (cc->sockfd > maxfd)
			maxfd = cc->sockfd;
		if (cc->vrtp) {
			//TODO add video rtcp sock to rfds
		}
		if (cc->artp) {
			//TODO add audio rtcp sock to rfds
		}
	}

	memset(&tv, 0, sizeof(tv));
	tv.tv_sec = 0;
	tv.tv_usec = 0;

	ret = select(maxfd + 1, &rfds, NULL, NULL, &tv);
	if (ret < 0) {
		err("select failed : %s\n", strerror(errno));
		return -1;
	}
	if (ret == 0) {
		return 0;
	}

	// 554端口的sockfd触发了，就是有新链接。否则就是已有连接来了新消息。
	if (FD_ISSET(d->sockfd, &rfds)) {
		//new client_connection
		rtsp_new_client_connection(d);//创建一个新的连接，并添加到连接队列中。其实就是accetp一个新的sockfd。。。
	}

	cc = TAILQ_FIRST(&d->connections_qhead); //NOTE do not use TAILQ_FOREACH
	while (cc) {//循环处理所有连接队列中连接消息。
		struct rtsp_client_connection *cc1 = cc;
		cc = TAILQ_NEXT(cc, demo_entry);

		if (cc1->vrtp) {
			//TODO process video rtcp socket
		}
		if (cc1->artp) {
			//TODO process audio rtcp socket
		}

		if (!FD_ISSET(cc1->sockfd, &rfds)) {
			continue;
		}

		rtsp_msg_init(&reqmsg);
		rtsp_msg_init(&resmsg);

		ret = rtsp_recv_msg(cc1, &reqmsg);
		if (ret == 0)
			continue;
		if (ret < 0) {
			rtsp_del_client_connection(cc1);
			continue;
		}

		if (reqmsg.type == RTSP_MSG_TYPE_INTERLEAVED) {
			//TODO process RTCP over TCP frame
			warn("TODO TODO TODO process interleaved frame\n");
			rtsp_msg_free(&reqmsg);
			continue;
		}

		if (reqmsg.type != RTSP_MSG_TYPE_REQUEST) {
			err("not request frame.\n");
			rtsp_msg_free(&reqmsg);
			continue;
		}

		ret = rtsp_process_request(cc1, &reqmsg, &resmsg);
		if (ret < 0) {
			err("request internal err\n");
		} else {
			rtsp_send_msg(cc1, &resmsg);
		}

		rtsp_msg_free(&reqmsg);
		rtsp_msg_free(&resmsg);
	}

	return 1;
}

static int rtp_tx_data (struct rtp_connection *c, const uint8_t *data, int size)
{
	int sockfd = c->udp_sockfd[0];
	int ret;
#if 0 //XXX
	uint8_t szbuf[RTP_MAX_PKTSIZ + 4];
	if (c->is_over_tcp) {
		sockfd = c->tcp_sockfd;
		szbuf[0] = '$';
		szbuf[1] = c->tcp_interleaved;
		*((uint16_t*)&szbuf[2]) = htons(size);
		memcpy(szbuf + 4, data, size);
		data = szbuf;
		size += 4;
	}
#else
	uint8_t szbuf[4];
	if (c->is_over_tcp) {
		sockfd = c->tcp_sockfd;
		szbuf[0] = '$';
		szbuf[1] = c->tcp_interleaved;
		*((uint16_t*)&szbuf[2]) = htons(size);
		ret = send(sockfd, (const char*)szbuf, 4, 0);
		if (ret < 0) {
			err("rtp send interlaced frame failed: %s\n", strerror(errno));
			return -1;
		}
	}
#endif

	ret = send(sockfd, (const char*)data, size, 0);
	if (ret < 0) {
		err("rtp send %d bytes failed: %s\n", size, strerror(errno));
		return -1;
	}

	return size;
}

static int rtsp_try_set_sps_pps (struct rtsp_session *s, const uint8_t *frame, int len)
{
	uint8_t type = 0;
	if (s->h264_sps_len > 0 && s->h264_pps_len > 0) {
		return 0;
	}

	if (frame[0] == 0 && frame[1] == 0 && frame[2] == 1) {
		type = frame[3] & 0x1f;
		frame += 3;
		len   -= 3;
	}
	if (frame[0] == 0 && frame[1] == 0 && frame[2] == 0 && frame[3] == 1) {
		type = frame[4] & 0x1f;
		frame += 4;
		len   -= 4;
	}

	if (len < 1)
		return -1;

	if (type == 7 && 0 == s->h264_sps_len) {
		dbg("sps %d\n", len);
		if (len > sizeof(s->h264_sps))
			len = sizeof(s->h264_sps);
		memcpy(s->h264_sps, frame, len);
		s->h264_sps_len = len;
	}

	if (type == 8 && 0 == s->h264_pps_len) {
		dbg("pps %d\n", len);
		if (len > sizeof(s->h264_pps))
			len = sizeof(s->h264_pps);
		memcpy(s->h264_pps, frame, len);
		s->h264_pps_len = len;
	}

	return 0;
}

static const uint8_t *rtsp_find_h264_nalu (const uint8_t *buff, int len, uint8_t *type, int *size) 
{
	const uint8_t *s = NULL;
	while (len >= 3) {
		if (buff[0] == 0 && buff[1] == 0 && buff[2] == 1) {
			if (!s) {
				if (len < 4)
					return NULL;
				s = buff;
				*type = buff[3] & 0x1f;
			} else {
				*size = (buff - s);
				return s;
			}
			buff += 3;
			len  -= 3;
			continue;
		}
		if (len >= 4 && buff[0] == 0 && buff[1] == 0 && buff[2] == 0 && buff[3] == 1) {
			if (!s) {
				if (len < 5)
					return NULL;
				s = buff;
				*type = buff[4] & 0x1f;
			} else {
				*size = (buff - s);
				return s;
			}
			buff += 4;
			len  -= 4;
			continue;
		}
		buff ++;
		len --;
	}
	if (!s)
		return NULL;
	*size = (buff - s + len);
	return s;
}

static int rtsp_tx_video_internal (struct rtsp_session *s, const uint8_t *frame, int len, uint64_t ts)
{
	struct rtsp_client_connection *cc = NULL;
	uint8_t *packets[RTP_MAX_NBPKTS] = {NULL};
	int pktsizs[RTP_MAX_NBPKTS] = {0};
	int count, i;

	rtsp_try_set_sps_pps(s, frame, len);

	count = rtp_enc_h264(&s->vrtpe, frame, len, ts, packets, pktsizs);
	if (count <= 0) {
		err("rtp_enc_h264 ret = %d\n", count);
		return -1;
	}

	TAILQ_FOREACH(cc, &s->connections_qhead, session_entry) {
		if (cc->state != RTSP_CC_STATE_PLAYING || !cc->vrtp)
			continue;

		for (i = 0; i < count; i++) {
			*((uint32_t*)(&packets[i][8])) = htonl(cc->vrtp->ssrc); //modify ssrc
			if (rtp_tx_data(cc->vrtp, packets[i], pktsizs[i]) < 0) {
				break;
			}
		}
	}
	return len;
}

int rtsp_tx_video (rtsp_session_handle session, const uint8_t *frame, int len, uint64_t ts)
{
	struct rtsp_session *s = (struct rtsp_session*) session;
//	struct rtsp_client_connection *cc = NULL;
	int ret = 0;

	if (!s || !frame || !s->has_video)
		return -1;

	//dbg("framelen = %d\n", len);
	while (len > 0) {
		const uint8_t *start = NULL;
		uint8_t type = 0;
		int size = 0;
		start = rtsp_find_h264_nalu(frame, len, &type, &size);
		if (!start) {
			warn("not found nal header\n");
			break;
		}
		//dbg("type:%u size:%d\n", type, size);
		ret += rtsp_tx_video_internal(s, start, size, ts);
		len -= (start - frame) + size;
		frame = start + size;
	}
	return ret;
}

int rtsp_tx_audio (rtsp_session_handle session, const uint8_t *frame, int len, uint64_t ts)
{
	struct rtsp_session *s = (struct rtsp_session*) session;
	struct rtsp_client_connection *cc = NULL;
	uint8_t *packets[RTP_MAX_NBPKTS] = {NULL};
	int pktsizs[RTP_MAX_NBPKTS] = {0};
	int count, i;

	if (!s || !frame || !s->has_audio)
		return -1;

	count = rtp_enc_g711(&s->artpe, frame, len, ts, packets, pktsizs);
	if (count <= 0) {
		err("rtp_enc_g711 ret = %d\n", count);
		return -1;
	}

	TAILQ_FOREACH(cc, &s->connections_qhead, session_entry) {
		if (cc->state != RTSP_CC_STATE_PLAYING || !cc->artp)
			continue;

		for (i = 0; i < count; i++) {
			*((uint32_t*)(&packets[i][8])) = htonl(cc->artp->ssrc); //modify ssrc
			if (rtp_tx_data(cc->artp, packets[i], pktsizs[i]) < 0) {
				break;
			}
		}
	}
	return len;
}
