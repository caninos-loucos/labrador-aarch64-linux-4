#include <linux/types.h>
#include <linux/delay.h>
#include <asm/string.h>
#include "hdmi.h"

#define PACKET_PERIOD 1

int hdmi_gen_spd_infoframe(struct hdmi_ip *ip)
{
	static uint8_t pkt[32];
	static char spdname[8] = "Vienna";
	static char spddesc[16] = "DTV SetTop Box";
	
	unsigned int checksum = 0;
	unsigned int i;
	
	/* clear buffer */
	for (i = 0; i < 32; i++)
		pkt[i] = 0;
		
	/* header */
	pkt[0] = 0x80 | 0x03;	/* HB0: Packet Type = 0x83 */
	pkt[1] = 1;		/* HB1: version = 1 */
	pkt[2] = 0x1f & 25;	/* HB2: len = 25 */
	pkt[3] = 0x00;		/* PB0: checksum = 0 */

	/*
	 * data
	 */

	/* Vendor Name, 8 bytes */
	memcpy(&pkt[4], spdname, 8);

	/* Product Description, 16 bytes */
	memcpy(&pkt[12], spddesc, 16);

	/* Source Device Information, Digital STB */
	pkt[28] = 0x1;

	/* checksum */
	for (i = 0; i < 31; i++)
		checksum += pkt[i];
	pkt[3] = (~checksum + 1)  & 0xff;

	/* set to RAM Packet */
	ip->ops->packet_generate(ip, PACKET_SPD_SLOT, pkt);
	ip->ops->packet_send(ip, PACKET_SPD_SLOT, PACKET_PERIOD);

	return 0;
}

/*
 * hdmi_gen_avi_infoframe
 *
 * input colorformat: 0--RGB444; 1--YCbCr422; 2--YCbCr444
 * AR: 1--4:3; 2--16:9
 */
int hdmi_gen_avi_infoframe(struct hdmi_ip *ip)
{
	static uint8_t pkt[32];
	uint8_t AR = 2;
	uint32_t checksum = 0, i = 0;

	/* clear buffer */
	for (i = 0; i < 32; i++)
		pkt[i] = 0;

	/* header */
	pkt[0] = 0x80 | 0x02;	/* header = 0x82 */
	pkt[1] = 2;		/* version = 2 */
	pkt[2] = 0x1f & 13;	/* len = 13 */
	pkt[3] = 0x00;		/* checksum = 0 */

	/*
	 * data
	 */

	/*
	 * PB1--Y1:Y0 = colorformat; R3:R1 is invalid;
	 * no bar info and scan info
	 */
	pkt[4] = (ip->settings.pixel_encoding << 5) | (0 << 4) | (0 << 2) | (0);

	/* 0--Normal YCC601 or YCC709; 1--xvYCC601; 2--xvYCC709 */
	if (ip->settings.color_xvycc == 0) {
		/*
		 * PB2--Colorimetry: SMPTE 170M | ITU601;
		 * Picture aspect Ratio; same as picture aspect ratio
		 */
		pkt[5] = (0x1<<6) | (AR << 4) | (0x8);

		/* PB3--No known non-uniform scaling */
		pkt[6] = 0x0;
	} else if (ip->settings.color_xvycc == 1) {
		/*
		 * PB2--Colorimetry: SMPTE 170M | ITU601;
		 * Picture aspect Ratio; same as picture aspect ratio
		 */
		pkt[5] = (0x3 << 6) | (AR << 4) | (0x8);

		/* PB3--xvYCC601;No known non-uniform scaling */
		pkt[6] = 0x0;

	} else {
		/*
		 * PB2--Colorimetry: SMPTE 170M | ITU601;
		 * Picture aspect Ratio; same as picture aspect ratio
		 */
		pkt[5] = (0x3 << 6) | (AR << 4) | (0x8);

		/* PB3--xvYCC709;No known non-uniform scaling */
		pkt[6] = 0x1 << 4;
	}

	/* PB4--Video Id */
	
		pkt[7] = ip->vid;

	/* PB5--Pixel repeat time */
	pkt[8] = 0;

	/* PB6--PB13: Bar Info, no bar info */
	pkt[9] = 0;
	pkt[10] = 0;
	pkt[11] = 0;
	pkt[12] = 0;
	pkt[13] = 0;
	pkt[14] = 0;
	pkt[15] = 0;
	pkt[16] = 0;

	/* checksum */
	for (i = 0; i < 31; i++)
		checksum += pkt[i];
	pkt[3] = (~checksum + 1) & 0xff;

	/* set to RAM Packet */
	ip->ops->packet_generate(ip, PACKET_AVI_SLOT, pkt);
	ip->ops->packet_send(ip, PACKET_AVI_SLOT, PACKET_PERIOD);

	return 0;
}

/*
 * hdmi_gen_gbd_infoframe
 *
 * input: Color_xvYCC: 0--Normal YCC601 or YCC709; 1--xvYCC601; 2--xvYCC709
 *	ColorDepth: 0--24bit; 1--30bit; 2--36bit
 * return: 0
 */
int hdmi_gen_gbd_infoframe(struct hdmi_ip *ip)
{
	static uint8_t pkt[32];

	int i;
	unsigned int deep_color = ip->settings.deep_color;
	unsigned int color_xvycc = ip->settings.color_xvycc;

	uint32_t checksum = 0;

	/* clear buffer */
	for (i = 0; i < 32; i++)
		pkt[i] = 0;

	/*
	 * header:
	 *	0: header
	 *	1: Next_Field = 1; GBD_Profile = P0; Affected Gamut seq num = 1
	 *	2: Only Packet in sequence; current Gamut seq num = 1
	 */
	pkt[0] = 0xa;
	pkt[1] = (0x1 << 7) | (0x0 << 4) | (0x1);
	pkt[2] = (0x3 << 4) | (0x1);
	pkt[3] = 0x00;

	/*
	 * data
	 */

	/* PB1--Format Flag; GBD_Color_Precision; GBD_Color_Space */

	pkt[4] = (0x1 << 7) | (deep_color << 3) | (color_xvycc);

	if (deep_color == 0) {
		/* 24bit */
		pkt[5] = 0x0;		/* min Red data */
		pkt[6] = 0xfe;		/* max Red data */
		pkt[7] = 0x0;		/* min Green data */
		pkt[8] = 0xfe;		/* max Green data */
		pkt[9] = 0x0;		/* min Blue data */
		pkt[10] = 0xfe;		/* max Blue data */
	} else if (deep_color == 1) {
		/* 32bit */
		pkt[5] = 0x0;
		pkt[6] = 0x3f;
		pkt[7] = 0x80;
		pkt[8] = 0x3;
		pkt[9] = 0xf8;
		pkt[10] = 0x0;
		pkt[11] = 0x3f;
		pkt[12] = 0x80;
	} else if (deep_color == 2) {
		/* 36bit */
		pkt[5] = 0x0;
		pkt[6] = 0xf;
		pkt[7] = 0xe0;
		pkt[8] = 0x0;
		pkt[9] = 0xf;
		pkt[10] = 0xe0;
		pkt[11] = 0x0;
		pkt[12] = 0xf;
		pkt[13] = 0xe0;
	}

	/* count checksum */
	for (i = 0; i < 31; i++)
		checksum += pkt[i];
	pkt[3] = (~checksum + 1) & 0xff;

	/* set to RAM Packet */
	ip->ops->packet_generate(ip, PACKET_GBD_SLOT, pkt);
	ip->ops->packet_send(ip, PACKET_GBD_SLOT, PACKET_PERIOD);

	return 0;
}

/*
 * hdmi_gen_vs_infoframe(Vendor Specific)
 * input:  3D format
 * return: 0
 */

int hdmi_gen_vs_infoframe(struct hdmi_ip *ip)
{
	static uint8_t pkt[32];
	uint32_t checksum = 0;
	int i;

	/* clear buffer */
	for (i = 0; i < 32; i++)
		pkt[i] = 0;

	/* header */
	pkt[0] = 0x81;	/* header */
	pkt[1] = 0x1;	/* Version */
	pkt[2] = 0x6;	/* length */
	pkt[3] = 0x00;	/* checksum */

	/*
	 * data
	 */

	/* PB1--PB3:24bit IEEE Registration Identifier */
	pkt[4] = 0x03;
	pkt[5] = 0x0c;
	pkt[6] = 0x00;


	if (ip->settings.mode_3d != 0)
	{
		pkt[7] = 0x2 << 5;	/* 3D format */

		switch (ip->settings.mode_3d) {
		case HDMI_3D_FRAME:
			pkt[8] = 0x0 << 4;
			pkt[9] = 0x0;
			break;

		case HDMI_3D_LR_HALF:
			pkt[8] = 0x8 << 4;
			pkt[9] = 0x1 << 4;
			break;

		case HDMI_3D_TB_HALF:
			pkt[8] = 0x6 << 4;
			pkt[9] = 0x0;
			break;

		default:
			break;
		}
	} else {
		/* not (3D and 4kx2k@24/25/30/24SMPTE) format, stop vs packet */
		ip->ops->packet_send(ip, PACKET_VS_SLOT, 0);
		return 0;
	}

	for (i = 0; i < 31; i++)
		checksum += pkt[i];
	pkt[3] = (~checksum + 1) & 0xff;

	/* set to RAM Packet */
	ip->ops->packet_generate(ip, PACKET_VS_SLOT, pkt);
	ip->ops->packet_send(ip, PACKET_VS_SLOT, PACKET_PERIOD);

	return 0;
}

int hdmi_packet_gen_infoframe(struct hdmi_ip *ip)
{
	hdmi_gen_spd_infoframe(ip);

	if (hdmi_gen_avi_infoframe(ip))
		return -1;

	/* hdmi_gen_gbd_infoframe(ip); */
	hdmi_gen_vs_infoframe(ip);

	return 0;
}

