/*************************************************************************
	> File Name: rtp_enc.c
	> Author: bxq
	> Mail: 544177215@qq.com 
	> Created Time: Saturday, December 19, 2015 PM09:16:04 CST
 ************************************************************************/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "comm.h"
#include "rtp_enc.h"

struct rtphdr
{
#ifdef __BIG_ENDIAN__
	uint16_t v:2;
	uint16_t p:1;
	uint16_t x:1;
	uint16_t cc:4;
	uint16_t m:1;
	uint16_t pt:7;
#else
	uint16_t cc:4;
	uint16_t x:1;
	uint16_t p:1;
	uint16_t v:2;
	uint16_t pt:7;
	uint16_t m:1;
#endif
	uint16_t seq;
	uint32_t ts;
	uint32_t ssrc;
};

#define RTPHDR_SIZE (12)

int rtp_enc_init (rtp_enc *e, uint16_t pktsiz, uint16_t nbpkts)
{
	if (!e || !pktsiz || !nbpkts)
		return -1;
	e->szbuf = (uint8_t*) malloc(pktsiz * nbpkts);
	if (!e->szbuf)
		return -1;
	e->pktsiz = pktsiz;
	e->nbpkts = nbpkts;
	return 0;
}

int rtp_enc_h264 (rtp_enc *e, const uint8_t *frame, int len, uint64_t ts, uint8_t *packets[], int pktsizs[])
{
	int count = 0;
	uint8_t nalhdr;
    uint32_t rtp_ts;

	if (!e || !e->szbuf)
		return -1;

	if (!frame || len <= 0 || !packets || !pktsizs)
		return -1;

	//drop 0001
	if (frame[0] == 0 && frame[1] == 0 && frame[2] == 1) {
		frame += 3;
		len   -= 3;
	}
	if (frame[0] == 0 && frame[1] == 0 && frame[2] == 0 && frame[3] == 1) {
		frame += 4;
		len   -= 4;
	}

	nalhdr = frame[0];
	rtp_ts = (uint32_t)(ts * e->sample_rate / 1000000);
// #define RTP_MAX_PKTSIZ	((1500-42)/4*4)//不超过1460bytes大小
// #define RTP_MAX_NBPKTS	(300)
	while (len > 0 && count < e->nbpkts) //5或者300包数据
	{
		struct rtphdr *hdr = NULL;
		int pktsiz = 0;
		packets[count] = e->szbuf + e->pktsiz * count;//添加每个rtp包的数据信息,rtp_hdr+rtp_body.
		hdr = (struct rtphdr*)(packets[count]);
		pktsiz = e->pktsiz;
		hdr->v = 2;
		hdr->p = 0;
		hdr->x = 0;
		hdr->cc = 0;
		hdr->m = 1;
		hdr->pt = e->pt;
		hdr->seq = htons(e->seq++);
		hdr->ts = htonl(rtp_ts);
		hdr->ssrc = htonl(e->ssrc);

		// 传入的数据len可以放在一包中进行发送。一个 rtp_hdr+nalu.
		if (count == 0 && len <= pktsiz - RTPHDR_SIZE) 
		{//
			memcpy(packets[count] + RTPHDR_SIZE, frame, len);
			pktsizs[count] = RTPHDR_SIZE + len;
			frame += len;
			len -= len;
		} else {//切片吧！FU-A,FU-A
			int mark = 0;
			if (count == 0) {
				frame ++; //drop nalu header
				len --;
			} else if (len <= pktsiz - RTPHDR_SIZE - 2) {
				mark = 1;
			}
			hdr->m = mark;

			packets[count][RTPHDR_SIZE + 0] = (nalhdr & 0xe0) | 28;//FU-A
			packets[count][RTPHDR_SIZE + 1] = (nalhdr & 0x1f);//FU-A
			if (count == 0) {
				packets[count][RTPHDR_SIZE + 1] |= 0x80; //S
			}

			if (mark) {
				packets[count][RTPHDR_SIZE + 1] |= 0x40; //E
				memcpy(packets[count] + RTPHDR_SIZE + 2, frame, len);
				pktsizs[count] = RTPHDR_SIZE + 2 + len;
				frame += len;
				len -= len;
			} else {
				memcpy(packets[count] + RTPHDR_SIZE + 2, frame, pktsiz - RTPHDR_SIZE - 2);
				pktsizs[count] = pktsiz;
				frame += pktsiz - RTPHDR_SIZE - 2;
				len -= pktsiz - RTPHDR_SIZE - 2;
			}
		}
		count ++;
	}
	return count;
}

int rtp_enc_g711 (rtp_enc *e, const uint8_t *frame, int len, uint64_t ts, uint8_t *packets[], int pktsizs[])
{
	int count = 0;
    uint32_t rtp_ts;

	if (!e || !e->szbuf)
		return -1;

	if (!frame || len <= 0 || !packets || !pktsizs)
		return -1;

	rtp_ts = (uint32_t)(ts * e->sample_rate / 1000000);//sample_count
	while (len > 0 && count < e->nbpkts) {
		struct rtphdr *hdr = NULL;
		int pktsiz = 0;
		packets[count] = e->szbuf + e->pktsiz * count;
		hdr = (struct rtphdr*)(packets[count]);
		pktsiz = e->pktsiz;
		hdr->v = 2;
		hdr->p = 0;
		hdr->x = 0;
		hdr->cc = 0;
		hdr->m = (e->seq == 0);
		hdr->pt = e->pt;
		hdr->seq = htons(e->seq++);
		hdr->ts  = htonl(rtp_ts);
		hdr->ssrc = htonl(e->ssrc);

		if (len <= pktsiz - RTPHDR_SIZE) {
			memcpy(packets[count] + RTPHDR_SIZE, frame, len);
			pktsizs[count] = RTPHDR_SIZE + len;
			frame += len;
			len -= len;
		} else {
			memcpy(packets[count] + RTPHDR_SIZE, frame, pktsiz - RTPHDR_SIZE);
			pktsizs[count] = pktsiz;
			frame += pktsiz - RTPHDR_SIZE;
			len -= pktsiz - RTPHDR_SIZE;
		}
		count ++;
	}

	return count;
}
/**
AAC封装RTP比较简单
将AAC的ADTS头去掉
12字节RTP头后紧跟着2个字节的AU_HEADER_LENGTH,
然后是2字节的AU_HEADER(2 bytes: 13 bits = length of frame, 3 bits = AU-Index(-delta)))，之后就是AAC payload。

所以要得到AACpayload
payLen= （UINT16）usAuheader >> 3;（这里要注意usAuheader的值，RTP中是网络序的，要转成主机序）
具体见RFC3640

static int aac_parse(Track *tr, uint8_t *data, size_t len)
{
	//XXX handle the last packet on EOF  
	int off = 0;
	uint32_t payload = DEFAULT_MTU - AU_HEADER_SIZE;
	uint8_t *packet = g_malloc0(DEFAULT_MTU);
	if (!packet) 
		return -1;
	//两个字节的AU_HEADER_SIZE
	packet[0] = 0x00;
	packet[1] = 0x10;
	//AU_HEADER
	packet[2] = (len & 0x1fe0) >> 5;
	packet[3] = (len & 0x1f) << 3;
	if (len > payload) {
		while (len > payload) {
			memcpy(packet + AU_HEADER_SIZE, data + off, payload);
			mparser_buffer_write(tr,
				tr->properties.pts,
				tr->properties.dts,
				tr->properties.frame_duration,
				0,
				packet, DEFAULT_MTU);
			len -= payload;
			off += payload;
		}
	}
	memcpy(packet + AU_HEADER_SIZE, data + off, len);
	mparser_buffer_write(tr,
		tr->properties.pts,
		tr->properties.dts,
		tr->properties.frame_duration,
		1,
		packet, len + AU_HEADER_SIZE);
	g_free(packet);
	return ERR_NOERROR;
}
*/
void rtp_enc_deinit (rtp_enc *e)
{
	if (e) {
		if (e->szbuf)
			free(e->szbuf);
		memset(e, 0, sizeof(rtp_enc));
	}
}

#if 0
int main()
{
	return 0;
}
#endif
