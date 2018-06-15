
#include"dshow.hpp"

const struct PixelFormatTag *avpriv_get_raw_pix_fmt_tags(void);
enum AVPixelFormat avpriv_find_pix_fmt(const PixelFormatTag *tags, unsigned int fourcc);
CameraInfoForUsb g_cameraInfo = { 0 };

#if 1
const PixelFormatTag  ff_raw_pix_fmt_tags[] = {
	{ AV_PIX_FMT_YUV420P, MKTAG('I', '4', '2', '0') }, /* Planar formats */
	{ AV_PIX_FMT_YUV420P, MKTAG('I', 'Y', 'U', 'V') },
	{ AV_PIX_FMT_YUV420P, MKTAG('Y', 'V', '1', '2') },
	{ AV_PIX_FMT_YUV410P, MKTAG('Y', 'U', 'V', '9') },
	{ AV_PIX_FMT_YUV410P, MKTAG('Y', 'V', 'U', '9') },
	{ AV_PIX_FMT_YUV411P, MKTAG('Y', '4', '1', 'B') },
	{ AV_PIX_FMT_YUV422P, MKTAG('Y', '4', '2', 'B') },
	{ AV_PIX_FMT_YUV422P, MKTAG('P', '4', '2', '2') },
	{ AV_PIX_FMT_YUV422P, MKTAG('Y', 'V', '1', '6') },
	/* yuvjXXX formats are deprecated hacks specific to libav*,
	they are identical to yuvXXX  */
	{ AV_PIX_FMT_YUVJ420P, MKTAG('I', '4', '2', '0') }, /* Planar formats */
	{ AV_PIX_FMT_YUVJ420P, MKTAG('I', 'Y', 'U', 'V') },
	{ AV_PIX_FMT_YUVJ420P, MKTAG('Y', 'V', '1', '2') },
	{ AV_PIX_FMT_YUVJ422P, MKTAG('Y', '4', '2', 'B') },
	{ AV_PIX_FMT_YUVJ422P, MKTAG('P', '4', '2', '2') },
	{ AV_PIX_FMT_GRAY8,    MKTAG('Y', '8', '0', '0') },
	{ AV_PIX_FMT_GRAY8,    MKTAG('Y', '8', ' ', ' ') },

	{ AV_PIX_FMT_YUYV422, MKTAG('Y', 'U', 'Y', '2') }, /* Packed formats */
	{ AV_PIX_FMT_YUYV422, MKTAG('Y', '4', '2', '2') },
	{ AV_PIX_FMT_YUYV422, MKTAG('V', '4', '2', '2') },
	{ AV_PIX_FMT_YUYV422, MKTAG('V', 'Y', 'U', 'Y') },
	{ AV_PIX_FMT_YUYV422, MKTAG('Y', 'U', 'N', 'V') },
	{ AV_PIX_FMT_YVYU422, MKTAG('Y', 'V', 'Y', 'U') }, /* Philips */
	{ AV_PIX_FMT_UYVY422, MKTAG('U', 'Y', 'V', 'Y') },
	{ AV_PIX_FMT_UYVY422, MKTAG('H', 'D', 'Y', 'C') },
	{ AV_PIX_FMT_UYVY422, MKTAG('U', 'Y', 'N', 'V') },
	{ AV_PIX_FMT_UYVY422, MKTAG('U', 'Y', 'N', 'Y') },
	{ AV_PIX_FMT_UYVY422, MKTAG('u', 'y', 'v', '1') },
	{ AV_PIX_FMT_UYVY422, MKTAG('2', 'V', 'u', '1') },
	{ AV_PIX_FMT_UYVY422, MKTAG('A', 'V', 'R', 'n') }, /* Avid AVI Codec 1:1 */
	{ AV_PIX_FMT_UYVY422, MKTAG('A', 'V', '1', 'x') }, /* Avid 1:1x */
	{ AV_PIX_FMT_UYVY422, MKTAG('A', 'V', 'u', 'p') },
	{ AV_PIX_FMT_UYVY422, MKTAG('V', 'D', 'T', 'Z') }, /* SoftLab-NSK VideoTizer */
	{ AV_PIX_FMT_UYVY422, MKTAG('a', 'u', 'v', '2') },
	{ AV_PIX_FMT_UYVY422, MKTAG('c', 'y', 'u', 'v') }, /* CYUV is also Creative YUV */
	{ AV_PIX_FMT_UYYVYY411, MKTAG('Y', '4', '1', '1') },
	{ AV_PIX_FMT_GRAY8,   MKTAG('G', 'R', 'E', 'Y') },
	{ AV_PIX_FMT_NV12,    MKTAG('N', 'V', '1', '2') },
	{ AV_PIX_FMT_NV21,    MKTAG('N', 'V', '2', '1') },

	/* nut */
	{ AV_PIX_FMT_RGB555LE, MKTAG('R', 'G', 'B', 15) },
	{ AV_PIX_FMT_BGR555LE, MKTAG('B', 'G', 'R', 15) },
	{ AV_PIX_FMT_RGB565LE, MKTAG('R', 'G', 'B', 16) },
	{ AV_PIX_FMT_BGR565LE, MKTAG('B', 'G', 'R', 16) },
	{ AV_PIX_FMT_RGB555BE, MKTAG(15 , 'B', 'G', 'R') },
	{ AV_PIX_FMT_BGR555BE, MKTAG(15 , 'R', 'G', 'B') },
	{ AV_PIX_FMT_RGB565BE, MKTAG(16 , 'B', 'G', 'R') },
	{ AV_PIX_FMT_BGR565BE, MKTAG(16 , 'R', 'G', 'B') },
	{ AV_PIX_FMT_RGB444LE, MKTAG('R', 'G', 'B', 12) },
	{ AV_PIX_FMT_BGR444LE, MKTAG('B', 'G', 'R', 12) },
	{ AV_PIX_FMT_RGB444BE, MKTAG(12 , 'B', 'G', 'R') },
	{ AV_PIX_FMT_BGR444BE, MKTAG(12 , 'R', 'G', 'B') },
	{ AV_PIX_FMT_RGBA64LE, MKTAG('R', 'B', 'A', 64) },
	{ AV_PIX_FMT_BGRA64LE, MKTAG('B', 'R', 'A', 64) },
	{ AV_PIX_FMT_RGBA64BE, MKTAG(64 , 'R', 'B', 'A') },
	{ AV_PIX_FMT_BGRA64BE, MKTAG(64 , 'B', 'R', 'A') },
	{ AV_PIX_FMT_RGBA,     MKTAG('R', 'G', 'B', 'A') },
	{ AV_PIX_FMT_RGB0,     MKTAG('R', 'G', 'B',  0) },
	{ AV_PIX_FMT_BGRA,     MKTAG('B', 'G', 'R', 'A') },
	{ AV_PIX_FMT_BGR0,     MKTAG('B', 'G', 'R',  0) },
	{ AV_PIX_FMT_ABGR,     MKTAG('A', 'B', 'G', 'R') },
	{ AV_PIX_FMT_0BGR,     MKTAG(0 , 'B', 'G', 'R') },
	{ AV_PIX_FMT_ARGB,     MKTAG('A', 'R', 'G', 'B') },
	{ AV_PIX_FMT_0RGB,     MKTAG(0 , 'R', 'G', 'B') },
	{ AV_PIX_FMT_RGB24,    MKTAG('R', 'G', 'B', 24) },
	{ AV_PIX_FMT_BGR24,    MKTAG('B', 'G', 'R', 24) },
	{ AV_PIX_FMT_YUV411P,  MKTAG('4', '1', '1', 'P') },
	{ AV_PIX_FMT_YUV422P,  MKTAG('4', '2', '2', 'P') },
	{ AV_PIX_FMT_YUVJ422P, MKTAG('4', '2', '2', 'P') },
	{ AV_PIX_FMT_YUV440P,  MKTAG('4', '4', '0', 'P') },
	{ AV_PIX_FMT_YUVJ440P, MKTAG('4', '4', '0', 'P') },
	{ AV_PIX_FMT_YUV444P,  MKTAG('4', '4', '4', 'P') },
	{ AV_PIX_FMT_YUVJ444P, MKTAG('4', '4', '4', 'P') },
	{ AV_PIX_FMT_MONOWHITE,MKTAG('B', '1', 'W', '0') },
	{ AV_PIX_FMT_MONOBLACK,MKTAG('B', '0', 'W', '1') },
	{ AV_PIX_FMT_BGR8,     MKTAG('B', 'G', 'R',  8) },
	{ AV_PIX_FMT_RGB8,     MKTAG('R', 'G', 'B',  8) },
	{ AV_PIX_FMT_BGR4,     MKTAG('B', 'G', 'R',  4) },
	{ AV_PIX_FMT_RGB4,     MKTAG('R', 'G', 'B',  4) },
	{ AV_PIX_FMT_RGB4_BYTE,MKTAG('B', '4', 'B', 'Y') },
	{ AV_PIX_FMT_BGR4_BYTE,MKTAG('R', '4', 'B', 'Y') },
	{ AV_PIX_FMT_RGB48LE,  MKTAG('R', 'G', 'B', 48) },
	{ AV_PIX_FMT_RGB48BE,  MKTAG(48, 'R', 'G', 'B') },
	{ AV_PIX_FMT_BGR48LE,  MKTAG('B', 'G', 'R', 48) },
	{ AV_PIX_FMT_BGR48BE,  MKTAG(48, 'B', 'G', 'R') },
	{ AV_PIX_FMT_GRAY16LE,    MKTAG('Y', '1',  0 , 16) },
	{ AV_PIX_FMT_GRAY16BE,    MKTAG(16 ,  0 , '1', 'Y') },
	{ AV_PIX_FMT_YUV420P10LE, MKTAG('Y', '3', 11 , 10) },
	{ AV_PIX_FMT_YUV420P10BE, MKTAG(10 , 11 , '3', 'Y') },
	{ AV_PIX_FMT_YUV422P10LE, MKTAG('Y', '3', 10 , 10) },
	{ AV_PIX_FMT_YUV422P10BE, MKTAG(10 , 10 , '3', 'Y') },
	{ AV_PIX_FMT_YUV444P10LE, MKTAG('Y', '3',  0 , 10) },
	{ AV_PIX_FMT_YUV444P10BE, MKTAG(10 ,  0 , '3', 'Y') },
	{ AV_PIX_FMT_YUV420P12LE, MKTAG('Y', '3', 11 , 12) },
	{ AV_PIX_FMT_YUV420P12BE, MKTAG(12 , 11 , '3', 'Y') },
	{ AV_PIX_FMT_YUV422P12LE, MKTAG('Y', '3', 10 , 12) },
	{ AV_PIX_FMT_YUV422P12BE, MKTAG(12 , 10 , '3', 'Y') },
	{ AV_PIX_FMT_YUV444P12LE, MKTAG('Y', '3',  0 , 12) },
	{ AV_PIX_FMT_YUV444P12BE, MKTAG(12 ,  0 , '3', 'Y') },
	{ AV_PIX_FMT_YUV420P14LE, MKTAG('Y', '3', 11 , 14) },
	{ AV_PIX_FMT_YUV420P14BE, MKTAG(14 , 11 , '3', 'Y') },
	{ AV_PIX_FMT_YUV422P14LE, MKTAG('Y', '3', 10 , 14) },
	{ AV_PIX_FMT_YUV422P14BE, MKTAG(14 , 10 , '3', 'Y') },
	{ AV_PIX_FMT_YUV444P14LE, MKTAG('Y', '3',  0 , 14) },
	{ AV_PIX_FMT_YUV444P14BE, MKTAG(14 ,  0 , '3', 'Y') },
	{ AV_PIX_FMT_YUV420P16LE, MKTAG('Y', '3', 11 , 16) },
	{ AV_PIX_FMT_YUV420P16BE, MKTAG(16 , 11 , '3', 'Y') },
	{ AV_PIX_FMT_YUV422P16LE, MKTAG('Y', '3', 10 , 16) },
	{ AV_PIX_FMT_YUV422P16BE, MKTAG(16 , 10 , '3', 'Y') },
	{ AV_PIX_FMT_YUV444P16LE, MKTAG('Y', '3',  0 , 16) },
	{ AV_PIX_FMT_YUV444P16BE, MKTAG(16 ,  0 , '3', 'Y') },
	{ AV_PIX_FMT_YUVA420P,    MKTAG('Y', '4', 11 ,  8) },
	{ AV_PIX_FMT_YUVA422P,    MKTAG('Y', '4', 10 ,  8) },
	{ AV_PIX_FMT_YUVA444P,    MKTAG('Y', '4',  0 ,  8) },
	{ AV_PIX_FMT_YA8,         MKTAG('Y', '2',  0 ,  8) },

	{ AV_PIX_FMT_YUVA420P9LE,  MKTAG('Y', '4', 11 ,  9) },
	{ AV_PIX_FMT_YUVA420P9BE,  MKTAG(9 , 11 , '4', 'Y') },
	{ AV_PIX_FMT_YUVA422P9LE,  MKTAG('Y', '4', 10 ,  9) },
	{ AV_PIX_FMT_YUVA422P9BE,  MKTAG(9 , 10 , '4', 'Y') },
	{ AV_PIX_FMT_YUVA444P9LE,  MKTAG('Y', '4',  0 ,  9) },
	{ AV_PIX_FMT_YUVA444P9BE,  MKTAG(9 ,  0 , '4', 'Y') },
	{ AV_PIX_FMT_YUVA420P10LE, MKTAG('Y', '4', 11 , 10) },
	{ AV_PIX_FMT_YUVA420P10BE, MKTAG(10 , 11 , '4', 'Y') },
	{ AV_PIX_FMT_YUVA422P10LE, MKTAG('Y', '4', 10 , 10) },
	{ AV_PIX_FMT_YUVA422P10BE, MKTAG(10 , 10 , '4', 'Y') },
	{ AV_PIX_FMT_YUVA444P10LE, MKTAG('Y', '4',  0 , 10) },
	{ AV_PIX_FMT_YUVA444P10BE, MKTAG(10 ,  0 , '4', 'Y') },
	{ AV_PIX_FMT_YUVA420P16LE, MKTAG('Y', '4', 11 , 16) },
	{ AV_PIX_FMT_YUVA420P16BE, MKTAG(16 , 11 , '4', 'Y') },
	{ AV_PIX_FMT_YUVA422P16LE, MKTAG('Y', '4', 10 , 16) },
	{ AV_PIX_FMT_YUVA422P16BE, MKTAG(16 , 10 , '4', 'Y') },
	{ AV_PIX_FMT_YUVA444P16LE, MKTAG('Y', '4',  0 , 16) },
	{ AV_PIX_FMT_YUVA444P16BE, MKTAG(16 ,  0 , '4', 'Y') },

	{ AV_PIX_FMT_GBRP,         MKTAG('G', '3', 00 ,  8) },
	{ AV_PIX_FMT_GBRP9LE,      MKTAG('G', '3', 00 ,  9) },
	{ AV_PIX_FMT_GBRP9BE,      MKTAG(9 , 00 , '3', 'G') },
	{ AV_PIX_FMT_GBRP10LE,     MKTAG('G', '3', 00 , 10) },
	{ AV_PIX_FMT_GBRP10BE,     MKTAG(10 , 00 , '3', 'G') },
	{ AV_PIX_FMT_GBRP12LE,     MKTAG('G', '3', 00 , 12) },
	{ AV_PIX_FMT_GBRP12BE,     MKTAG(12 , 00 , '3', 'G') },
	{ AV_PIX_FMT_GBRP14LE,     MKTAG('G', '3', 00 , 14) },
	{ AV_PIX_FMT_GBRP14BE,     MKTAG(14 , 00 , '3', 'G') },
	{ AV_PIX_FMT_GBRP16LE,     MKTAG('G', '3', 00 , 16) },
	{ AV_PIX_FMT_GBRP16BE,     MKTAG(16 , 00 , '3', 'G') },

	{ AV_PIX_FMT_XYZ12LE,      MKTAG('X', 'Y', 'Z' , 36) },
	{ AV_PIX_FMT_XYZ12BE,      MKTAG(36 , 'Z' , 'Y', 'X') },

	{ AV_PIX_FMT_BAYER_BGGR8,    MKTAG(0xBA, 'B', 'G', 8) },
	{ AV_PIX_FMT_BAYER_BGGR16LE, MKTAG(0xBA, 'B', 'G', 16) },
	{ AV_PIX_FMT_BAYER_BGGR16BE, MKTAG(16  , 'G', 'B', 0xBA) },
	{ AV_PIX_FMT_BAYER_RGGB8,    MKTAG(0xBA, 'R', 'G', 8) },
	{ AV_PIX_FMT_BAYER_RGGB16LE, MKTAG(0xBA, 'R', 'G', 16) },
	{ AV_PIX_FMT_BAYER_RGGB16BE, MKTAG(16  , 'G', 'R', 0xBA) },
	{ AV_PIX_FMT_BAYER_GBRG8,    MKTAG(0xBA, 'G', 'B', 8) },
	{ AV_PIX_FMT_BAYER_GBRG16LE, MKTAG(0xBA, 'G', 'B', 16) },
	{ AV_PIX_FMT_BAYER_GBRG16BE, MKTAG(16,   'B', 'G', 0xBA) },
	{ AV_PIX_FMT_BAYER_GRBG8,    MKTAG(0xBA, 'G', 'R', 8) },
	{ AV_PIX_FMT_BAYER_GRBG16LE, MKTAG(0xBA, 'G', 'R', 16) },
	{ AV_PIX_FMT_BAYER_GRBG16BE, MKTAG(16,   'R', 'G', 0xBA) },

	/* quicktime */
	{ AV_PIX_FMT_YUV420P, MKTAG('R', '4', '2', '0') }, /* Radius DV YUV PAL */
	{ AV_PIX_FMT_YUV411P, MKTAG('R', '4', '1', '1') }, /* Radius DV YUV NTSC */
	{ AV_PIX_FMT_UYVY422, MKTAG('2', 'v', 'u', 'y') },
	{ AV_PIX_FMT_UYVY422, MKTAG('2', 'V', 'u', 'y') },
	{ AV_PIX_FMT_UYVY422, MKTAG('A', 'V', 'U', 'I') }, /* FIXME merge both fields */
	{ AV_PIX_FMT_UYVY422, MKTAG('b', 'x', 'y', 'v') },
	{ AV_PIX_FMT_YUYV422, MKTAG('y', 'u', 'v', '2') },
	{ AV_PIX_FMT_YUYV422, MKTAG('y', 'u', 'v', 's') },
	{ AV_PIX_FMT_YUYV422, MKTAG('D', 'V', 'O', 'O') }, /* Digital Voodoo SD 8 Bit */
	{ AV_PIX_FMT_RGB555LE,MKTAG('L', '5', '5', '5') },
	{ AV_PIX_FMT_RGB565LE,MKTAG('L', '5', '6', '5') },
	{ AV_PIX_FMT_RGB565BE,MKTAG('B', '5', '6', '5') },
	{ AV_PIX_FMT_BGR24,   MKTAG('2', '4', 'B', 'G') },
	{ AV_PIX_FMT_BGR24,   MKTAG('b', 'x', 'b', 'g') },
	{ AV_PIX_FMT_BGRA,    MKTAG('B', 'G', 'R', 'A') },
	{ AV_PIX_FMT_RGBA,    MKTAG('R', 'G', 'B', 'A') },
	{ AV_PIX_FMT_RGB24,   MKTAG('b', 'x', 'r', 'g') },
	{ AV_PIX_FMT_ABGR,    MKTAG('A', 'B', 'G', 'R') },
	{ AV_PIX_FMT_GRAY16BE,MKTAG('b', '1', '6', 'g') },
	{ AV_PIX_FMT_RGB48BE, MKTAG('b', '4', '8', 'r') },

	/* special */
	{ AV_PIX_FMT_RGB565LE,MKTAG(3 ,  0 ,  0 ,  0) }, /* flipped RGB565LE */
	{ AV_PIX_FMT_YUV444P, MKTAG('Y', 'V', '2', '4') }, /* YUV444P, swapped UV */

	{ AV_PIX_FMT_NONE, 0 },
};
AVPixFmtDescriptor g_av_pix_fmt_descriptors[AV_PIX_FMT_NB];

void init_av_pix_fmt_descriptors()
{
	g_av_pix_fmt_descriptors[AV_PIX_FMT_YUV420P].name = "yuv420p";
	g_av_pix_fmt_descriptors[AV_PIX_FMT_YUV420P].nb_components = 3;
	g_av_pix_fmt_descriptors[AV_PIX_FMT_YUV420P].log2_chroma_w = 1;
	g_av_pix_fmt_descriptors[AV_PIX_FMT_YUV420P].log2_chroma_h = 1;
	//av_pix_fmt_descriptors[AV_PIX_FMT_YUV420P].comp = {{ 0, 0, 1, 0, 7 },        /* Y */{ 1, 0, 1, 0, 7 },        /* U */{ 2, 0, 1, 0, 7 },        /* V */};
	g_av_pix_fmt_descriptors[AV_PIX_FMT_YUV420P].flags = AV_PIX_FMT_FLAG_PLANAR;

	g_av_pix_fmt_descriptors[AV_PIX_FMT_YUYV422].name = "yuyv422";
	g_av_pix_fmt_descriptors[AV_PIX_FMT_YUYV422].nb_components = 3;
	g_av_pix_fmt_descriptors[AV_PIX_FMT_YUYV422].log2_chroma_w = 1;
	g_av_pix_fmt_descriptors[AV_PIX_FMT_YUYV422].log2_chroma_h = 0;
	//	 av_pix_fmt_descriptors[AV_PIX_FMT_YUYV422].comp = {{ 0, 1, 1, 0, 7 },        /* Y */{ 0, 3, 2, 0, 7 },        /* U */{ 0, 3, 4, 0, 7 },        /* V */};



	g_av_pix_fmt_descriptors[AV_PIX_FMT_YVYU422].name = "yvyu422";
	g_av_pix_fmt_descriptors[AV_PIX_FMT_YVYU422].nb_components = 3;
	g_av_pix_fmt_descriptors[AV_PIX_FMT_YVYU422].log2_chroma_w = 1;
	g_av_pix_fmt_descriptors[AV_PIX_FMT_YVYU422].log2_chroma_h = 0;
	//              av_pix_fmt_descriptors[AV_PIX_FMT_YVYU422].comp = {{ 0, 1, 1, 0, 7 },        /* Y */{ 0, 3, 2, 0, 7 },        /* V */{ 0, 3, 4, 0, 7 },        /* U */};

	g_av_pix_fmt_descriptors[AV_PIX_FMT_RGB24].name = "rgb24";
	g_av_pix_fmt_descriptors[AV_PIX_FMT_RGB24].nb_components = 3;
	g_av_pix_fmt_descriptors[AV_PIX_FMT_RGB24].log2_chroma_w = 0;
	g_av_pix_fmt_descriptors[AV_PIX_FMT_RGB24].log2_chroma_h = 0;
	//              av_pix_fmt_descriptors[AV_PIX_FMT_RGB24].comp = {{ 0, 2, 1, 0, 7 },        /* R */{ 0, 2, 2, 0, 7 },        /* G */{ 0, 2, 3, 0, 7 },        /* B */};
	g_av_pix_fmt_descriptors[AV_PIX_FMT_RGB24].flags = AV_PIX_FMT_FLAG_RGB;

}
const struct PixelFormatTag *avpriv_get_raw_pix_fmt_tags(void)
{
	return ff_raw_pix_fmt_tags;
}
#endif

static char *dup_wchar_to_utf8(wchar_t *w)
{
	char *s = NULL;
	int l = WideCharToMultiByte(CP_UTF8, 0, w, -1, 0, 0, 0, 0);
	s = (char *)malloc(l * sizeof(char));
	if (s)
		WideCharToMultiByte(CP_UTF8, 0, w, -1, s, l, 0, 0);
	return s;
}
enum AVPixelFormat dshow_pixfmt(unsigned long biCompression, unsigned short biBitCount)
{
	switch (biCompression) {
		/*case BI_BITFIELDS:
		case BI_RGB:*/
	case 0L:
	case 3L:
		switch (biBitCount) { /* 1-8 are untested */
		case 1:
			return AV_PIX_FMT_MONOWHITE;
		case 4:
			return AV_PIX_FMT_RGB4;
		case 8:
			return AV_PIX_FMT_RGB8;
		case 16:
			return AV_PIX_FMT_RGB555;
		case 24:
			return AV_PIX_FMT_BGR24;
		case 32:
			return AV_PIX_FMT_0RGB32;
		}
	}
	return avpriv_find_pix_fmt(avpriv_get_raw_pix_fmt_tags(), biCompression); // all others

}

int  dshow_cycle_formats_camera(IPin *pin)
{
	PixelInfo tmpPixelInfo = { 0 };
	IAMStreamConfig *config = NULL;
	int i, n, size;
	void *caps = NULL;
	HRESULT hr = S_OK;
	AM_MEDIA_TYPE *type = NULL;
	if (pin->QueryInterface(IID_IAMStreamConfig, (void **)&config) != S_OK)
		return -1;
	if (config->GetNumberOfCapabilities(&n, &size) != S_OK)
		goto end;


	caps = malloc(size);
	if (!caps)
		goto end;
	for (i = 0; i < n; i++)
	{
		memset(&tmpPixelInfo, 0, sizeof(PixelInfo));
		hr = config->GetStreamCaps(i, &type, (BYTE *)caps);
		if (hr != S_OK)
		{
			printf("error !!!11-1\n");
			goto next;;
		}
		VIDEO_STREAM_CONFIG_CAPS *vcaps = (VIDEO_STREAM_CONFIG_CAPS *)caps;

		BITMAPINFOHEADER *bih;
		INT64 *fr;
		if (IsEqualGUID(type->formattype, FORMAT_VideoInfo)) {
			VIDEOINFOHEADER *v = (VIDEOINFOHEADER *)type->pbFormat;
			fr = &v->AvgTimePerFrame;
			bih = &v->bmiHeader;
		}/* else if (IsEqualGUID(&type->formattype, &FORMAT_VideoInfo2)) {
		 VIDEOINFOHEADER2 *v = (void *) type->pbFormat;
		 fr = &v->AvgTimePerFrame;
		 bih = &v->bmiHeader;
		 }*/ else {
			goto next;
		}
		enum AVPixelFormat pix_fmt = dshow_pixfmt(bih->biCompression, bih->biBitCount);
		if (pix_fmt == AV_PIX_FMT_NONE)
		{
			goto next;
		}
		else
		{
			//			printf( "  pixel_format=%s", av_get_pix_fmt_name(pix_fmt));
			strcpy(tmpPixelInfo.formatName, av_get_pix_fmt_name(pix_fmt));


		}

		/*	if (type->subtype == MEDIASUBTYPE_YV12 ||
		type->subtype == MEDIASUBTYPE_RGB565 ||
		type->subtype == MEDIASUBTYPE_UYVY )  */
		{

			//printf( "index = %d, width = %d, height = %d\n",i,  vcaps->MaxOutputSize.cx,  vcaps->MaxOutputSize.cy); 
			/*		printf( "  min s=%ldx%ld fps=%g max s=%ldx%ld fps=%g\n",
			vcaps->MinOutputSize.cx, vcaps->MinOutputSize.cy,
			1e7 / vcaps->MaxFrameInterval,
			vcaps->MaxOutputSize.cx, vcaps->MaxOutputSize.cy,
			1e7 /vcaps->MinFrameInterval);*/
			// to correct the value;              


		}
		tmpPixelInfo.caps = *vcaps;

		g_cameraInfo.nCameraInfo[g_cameraInfo.nCount].formatInfo.push_back(tmpPixelInfo);

	next:
		if (type->pbFormat)
			CoTaskMemFree(type->pbFormat);
		CoTaskMemFree(type);
	}

end:
	config->Release();
	if (caps)
		free(caps);

	return 0;
}
int getCameraOtherInfo(IBaseFilter *device_filter)
{
	HRESULT hr;
	IPin *device_pin = NULL;
	IEnumPins *pins = 0;
	hr = device_filter->EnumPins(&pins);
	if (FAILED(hr))
	{
		return -1;
	}
	IPin *pin;
	while (pins->Next(1, &pin, NULL) == S_OK)
	{
		IKsPropertySet *p = NULL;
		IEnumMediaTypes *types = NULL;
		PIN_INFO info = { 0 };
		AM_MEDIA_TYPE *type;
		GUID category;
		DWORD r2;
		pin->QueryPinInfo(&info);
		info.pFilter->Release();
		if (info.dir != PINDIR_OUTPUT)
			goto next;
		if (pin->QueryInterface(IID_IKsPropertySet, (void **)&p) != S_OK)
			goto next;
		if (p->Get(AMPROPSETID_Pin, AMPROPERTY_PIN_CATEGORY,
			NULL, 0, &category, sizeof(GUID), &r2) != S_OK)
			goto next;
		if (!IsEqualGUID(category, PIN_CATEGORY_CAPTURE))
			goto next;


		char *buf = dup_wchar_to_utf8(info.achName);

		//	printf( " Pin \"%s\"\n", buf);
		dshow_cycle_formats_camera(pin);





		free(buf);



	next:

		if (p)
		{
			p->Release();
		}
		if (device_pin != pin)
		{
			pin->Release();
		}

	}
	pins->Release();



}

enum AVPixelFormat avpriv_find_pix_fmt(const PixelFormatTag *tags,
	unsigned int fourcc)
{
	while (tags->pix_fmt >= 0) {
		if (tags->fourcc == fourcc)
			return tags->pix_fmt;
		tags++;
	}
	return AV_PIX_FMT_NONE;
}




int get_all_CameraInfo(void)
{
	HRESULT hr;
	IGraphBuilder *m_pGraphBuilder;
	int i = 0;

	//Create the filter graph   
	if (FAILED(CoInitialize(NULL)))    //初始化COM库
	{
		return -1;
	}
	hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER,
		IID_IGraphBuilder, (LPVOID*)&m_pGraphBuilder);   /*建立滤镜管理器*/
	if (FAILED(hr))
	{
		return -1;
	}

	ICreateDevEnum *pDevEnum;
	hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER,
		IID_ICreateDevEnum, (LPVOID*)&pDevEnum); /*创建枚举器组件*/
	if (FAILED(hr))
	{
		return -1;
	}

	IEnumMoniker *classenum = NULL;

	const GUID *device_guid[1] = { &CLSID_VideoInputDeviceCategory };
	int r;

	hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &classenum, 0);   /*指定某一类型设备*/
	if (hr == S_FALSE)
	{
		hr = VFW_E_NOT_FOUND;
		return hr;
	}
	classenum->Reset();
	unsigned long cFetched;
	IMoniker *m = NULL;
	//	清空全局变量
	g_cameraInfo.nCount = 0;
	for (int j = 0; j< MAX_CAMERAS_NUM; j++)
	{
		vector <PixelInfo>().swap(g_cameraInfo.nCameraInfo[j].formatInfo);
		memset(g_cameraInfo.nCameraInfo[j].cameraName, 0, strlen(g_cameraInfo.nCameraInfo[j].cameraName));
	}

	i = 0;
	while (hr = classenum->Next(1, &m, &cFetched), hr == S_OK)
	{
		IPropertyBag *bag = NULL;
		char *buf = NULL;
		VARIANT var;

		hr = m->BindToStorage(0, 0, IID_IPropertyBag, (LPVOID*)&bag); //获取设备名字 

		if (FAILED(hr))
			goto fail1;

		var.vt = VT_BSTR;
		hr = bag->Read(L"FriendlyName", &var, NULL);  //检索过滤器的友好名字
		if (FAILED(hr))
			goto fail1;

		buf = dup_wchar_to_utf8(var.bstrVal);

		//	printf( " \"%s\"\n", buf);
		g_cameraInfo.nCount = i;
		strcpy(g_cameraInfo.nCameraInfo[i].cameraName, buf);

	fail1:
		if (buf)
			free(buf);
		if (bag)

			bag->Release();


		IBaseFilter *device_filter = NULL;

		m->BindToObject(0, 0, IID_IBaseFilter, (LPVOID*)&device_filter); //生成绑定到设备的filter


		if (NULL != device_filter)
		{
			getCameraOtherInfo(device_filter);
		}
		i++;

	}

	g_cameraInfo.nCount = i;
	classenum->Release();
	pDevEnum->Release();
	m_pGraphBuilder->Release();
	CoUninitialize();

	return 0;
}

CameraInfoForUsb *getCameraInfoForUsb()
{

	static int nflag = 1;

	if (nflag)
	{
		get_all_CameraInfo();
		nflag = 0;
	}
	return &g_cameraInfo;
}

void show_CamerInfo()
{

	CameraInfoForUsb *pCameraInfo = getCameraInfoForUsb();
	if (pCameraInfo->nCount == 0)
	{
		printf("there no camera connect pc\n");
		return;
	}

	printf("there is %d cameras connect pc\n", pCameraInfo->nCount);


	for (int i = 0; i < pCameraInfo->nCount; i++)
	{

		printf(" \"%s\"\n", pCameraInfo->nCameraInfo[i].cameraName);
		for (int j = 0; j <pCameraInfo->nCameraInfo[i].formatInfo.size(); j++)
		{
			printf("  pixel_format=%s", pCameraInfo->nCameraInfo[i].formatInfo[j].formatName);

			printf("  min s=%ldx%ld fps=%g max s=%ldx%ld fps=%g\n",
				pCameraInfo->nCameraInfo[i].formatInfo[j].caps.MinOutputSize.cx, pCameraInfo->nCameraInfo[i].formatInfo[j].caps.MinOutputSize.cy,
				1e7 / pCameraInfo->nCameraInfo[i].formatInfo[j].caps.MaxFrameInterval,
				pCameraInfo->nCameraInfo[i].formatInfo[j].caps.MaxOutputSize.cx, pCameraInfo->nCameraInfo[i].formatInfo[j].caps.MaxOutputSize.cy,
				1e7 / pCameraInfo->nCameraInfo[i].formatInfo[j].caps.MinFrameInterval);
		}

	}
}

int getCameraDevNum()
{

	CameraInfoForUsb *pCameraInfo = getCameraInfoForUsb();
	return pCameraInfo->nCount;
}


char * getCameraNameByIndex(int nIndex)
{
	CameraInfoForUsb *pCameraInfo = getCameraInfoForUsb();


	return pCameraInfo->nCameraInfo[nIndex - 1].cameraName;

}


int mainTest(int argc, char *argv[])
{
	char devName[64];
	int nIndex = 0;
	strcpy(devName, "video=");
	//init_av_pix_fmt_descriptors();
	show_CamerInfo();
	int nDevCnt = getCameraDevNum();
	if (nDevCnt == 0)
	{
		printf("error : no camera device\n");

		return -1;
	}
	if (nDevCnt == 1)
	{
		printf("there is only one camera connect pc \n");
	}
	else
	{
		printf("please in put the num camera you choose and must <= %d\n", nDevCnt);

	}
	while (1)
	{
		scanf("%d", &nIndex);

		if (nIndex <= 0 || nIndex > nDevCnt)
		{
			printf("error : what you put is invalid\n");
			break;
		}

		strcpy(devName, getCameraNameByIndex(nIndex));
		printf("devName=%s\n", devName);
	}
	
	system("pause");


	return 0;
}


