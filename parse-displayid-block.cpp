// SPDX-License-Identifier: MIT
/*
 * Copyright 2006-2012 Red Hat, Inc.
 * Copyright 2018-2019 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 *
 * Author: Adam Jackson <ajax@nwnk.net>
 * Maintainer: Hans Verkuil <hverkuil-cisco@xs4all.nl>
 */

#include "edid-decode.h"

static void parse_displayid_detailed_timing(const unsigned char *x)
{
	struct timings t = {};
	unsigned hbl, vbl;
	std::string s("aspect ");

	switch (x[3] & 0xf) {
	case 0:
		s += "1:1";
		t.ratio_w = t.ratio_h = 1;
		break;
	case 1:
		s += "5:4";
		t.ratio_w = 5;
		t.ratio_h = 4;
		break;
	case 2:
		s += "4:3";
		t.ratio_w = 4;
		t.ratio_h = 3;
		break;
	case 3:
		s += "15:9";
		t.ratio_w = 15;
		t.ratio_h = 9;
		break;
	case 4:
		s += "16:9";
		t.ratio_w = 16;
		t.ratio_h = 9;
		break;
	case 5:
		s += "16:10";
		t.ratio_w = 16;
		t.ratio_h = 10;
		break;
	case 6:
		s += "64:27";
		t.ratio_w = 64;
		t.ratio_h = 27;
		break;
	case 7:
		s += "256:135";
		t.ratio_w = 256;
		t.ratio_h = 135;
		break;
	default:
		s += "undefined";
		fail("Unknown aspect 0x%02x\n", x[3] & 0xf);
		break;
	}
	switch ((x[3] >> 5) & 0x3) {
	case 0:
		s += ", no 3D stereo";
		break;
	case 1:
		s += ", 3D stereo";
		break;
	case 2:
		s += ", 3D stereo depends on user action";
		break;
	case 3:
		s += ", reserved";
		fail("Reserved stereo 0x03\n");
		break;
	}
	if (x[3] & 0x10) {
		t.interlaced = true;
		s += ", interlaced";
	}
	if (x[3] & 0x80)
		s += ", preferred";

	t.pixclk_khz = 10 * (1 + (x[0] + (x[1] << 8) + (x[2] << 16)));
	t.w = 1 + (x[4] | (x[5] << 8));
	hbl = 1 + (x[6] | (x[7] << 8));
	t.hfp = 1 + (x[8] | ((x[9] & 0x7f) << 8));
	t.hsync = 1 + (x[10] | (x[11] << 8));
	t.hbp = hbl - t.hfp - t.hsync;
	if ((x[9] >> 7) & 0x1)
		t.pos_pol_hsync = true;
	t.h = 1 + (x[12] | (x[13] << 8));
	vbl = 1 + (x[14] | (x[15] << 8));
	t.vfp = 1 + (x[16] | ((x[17] & 0x7f) << 8));
	t.vsync = 1 + (x[18] | (x[19] << 8));
	t.vbp = vbl - t.vfp - t.vsync;
	if ((x[17] >> 7) & 0x1)
		t.pos_pol_vsync = true;
	
	printf("    Detailed mode: Clock %.3f MHz, %s\n"
	       "                   %4u %4u %4u %4u (%3u %3u %3d)\n"
	       "                   %4u %4u %4u %4u (%3u %3u %3d)\n"
	       "                   %chsync %cvsync\n"
	       "                   VertFreq: %.3f Hz, HorFreq: %.3f kHz\n",
	       (double)t.pixclk_khz/1000.0, s.c_str(),
	       t.w, t.w + t.hbp, t.w + t.hbp + t.hsync, t.w + hbl, t.hfp, t.hsync, t.hbp,
	       t.h, t.h + t.vbp, t.h + t.vbp + t.vsync, t.h + vbl, t.vfp, t.vsync, t.vbp,
	       t.pos_pol_hsync ? '+' : '-', t.pos_pol_vsync ? '+' : '-',
	       (t.pixclk_khz * 1000.0) / ((t.w + hbl) * (t.h + vbl)),
	       (double)(t.pixclk_khz) / (t.w + hbl)
	      );
}

void edid_state::parse_displayid_block(const unsigned char *x)
{
	const unsigned char *orig = x;
	unsigned version = x[1];
	int length = x[2];
	unsigned ext_count = x[4];
	unsigned i;

	printf("%s Version %u.%u Length %u Extension Count %u\n",
	       block.c_str(), version >> 4, version & 0xf,
	       length, ext_count);

	unsigned offset = 5;
	while (length > 0) {
		unsigned tag = x[offset];
		unsigned len = x[offset + 2];

		if (!tag && !len) {
			while (length && !x[offset]) {
				length--;
				offset++;
			}
			if (length)
				fail("Non-0 filler bytes in the DisplayID block\n");
			break;
		}
		switch (tag) {
		case 0x00: data_block = "Product ID Data Block"; break;
		case 0x01: data_block = "Display Parameters Data Block"; break;
		case 0x02: data_block = "Color Characteristics Data Block"; break;
		case 0x03: data_block = "Type 1 Detailed Timings Data Block"; break;
		case 0x04: data_block = "Type 2 Detailed Timings Data Block"; break;
		case 0x05: data_block = "Type 3 Short Timings Data Block"; break;
		case 0x06: data_block = "Type 4 DMT Timings Data Block"; break;
		case 0x07: data_block = "Type 1 VESA DMT Timings Data Block"; break;
		case 0x08: data_block = "CTA Timings Data Block"; break;
		case 0x09: data_block = "Video Timing Range Data Block"; break;
		case 0x0a: data_block = "Product Serial Number Data Block"; break;
		case 0x0b: data_block = "GP ASCII String Data Block"; break;
		case 0x0c: data_block = "Display Device Data Data Block"; break;
		case 0x0d: data_block = "Interface Power Sequencing Data Block"; break;
		case 0x0e: data_block = "Transfer Characteristics Data Block"; break;
		case 0x0f: data_block = "Display Interface Data Block"; break;
		case 0x10: data_block = "Stereo Display Interface Data Block"; break;
		case 0x12: data_block = "Tiled Display Topology Data Block"; break;
		default: data_block = "Unknown DisplayID Data Block (" + utohex(tag) + ")"; break;
		}

		printf("  %s\n", data_block.c_str());

		switch (tag) {
		case 0x03:
			for (i = 0; i < len / 20; i++) {
				parse_displayid_detailed_timing(&x[offset + 3 + (i * 20)]);
			}
			break;

		case 0x07:
			for (i = 0; i < min(len, 10) * 8; i++)
				if (x[offset + 3 + i / 8] & (1 << (i % 8)))
					print_timings("    ", find_dmt_id(i + 1), "DMT");
			break;

		case 0x12: {
			unsigned capabilities = x[offset + 3];
			unsigned num_v_tile = (x[offset + 4] & 0xf) | (x[offset + 6] & 0x30);
			unsigned num_h_tile = (x[offset + 4] >> 4) | ((x[offset + 6] >> 2) & 0x30);
			unsigned tile_v_location = (x[offset + 5] & 0xf) | ((x[offset + 6] & 0x3) << 4);
			unsigned tile_h_location = (x[offset + 5] >> 4) | (((x[offset + 6] >> 2) & 0x3) << 4);
			unsigned tile_width = x[offset + 7] | (x[offset + 8] << 8);
			unsigned tile_height = x[offset + 9] | (x[offset + 10] << 8);
			unsigned pix_mult = x[offset + 11];

			printf("    Capabilities: 0x%08x\n", capabilities);
			printf("    Num horizontal tiles: %u Num vertical tiles: %u\n", num_h_tile + 1, num_v_tile + 1);
			printf("    Tile location: %u, %u\n", tile_h_location, tile_v_location);
			printf("    Tile resolution: %ux%u\n", tile_width + 1, tile_height + 1);
			if (capabilities & 0x40) {
				if (pix_mult) {
					printf("    Top bevel size: %u pixels\n",
					       pix_mult * x[offset + 12] / 10);
					printf("    Bottom bevel size: %u pixels\n",
					       pix_mult * x[offset + 13] / 10);
					printf("    Right bevel size: %u pixels\n",
					       pix_mult * x[offset + 14] / 10);
					printf("    Left bevel size: %u pixels\n",
					       pix_mult * x[offset + 15] / 10);
				} else {
					fail("No bevel information, but the pixel multiplier is non-zero\n");
				}
				printf("    Tile resolution: %ux%u\n", tile_width + 1, tile_height + 1);
			} else if (pix_mult) {
				fail("No bevel information, but the pixel multiplier is non-zero\n");
			}
			break;
		}

		default:
			hex_block("    ", x + offset + 3, len);
			break;
		}
		length -= len + 3;
		offset += len + 3;
	}

	/*
	 * DisplayID length field is number of following bytes
	 * but checksum is calculated over the entire structure
	 * (excluding DisplayID-in-EDID magic byte)
	 */
	data_block.clear();
	do_checksum("  ", orig + 1, orig[2] + 5);
}
