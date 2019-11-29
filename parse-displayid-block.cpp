// SPDX-License-Identifier: MIT
/*
 * Copyright 2006-2012 Red Hat, Inc.
 * Copyright 2018-2019 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 *
 * Author: Adam Jackson <ajax@nwnk.net>
 * Maintainer: Hans Verkuil <hverkuil-cisco@xs4all.nl>
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <ctype.h>
#include <math.h>

#include <string>

#include "edid-decode.h"

static void parse_displayid_detailed_timing(const unsigned char *x)
{
	unsigned ha, hbl, hso, hspw;
	unsigned va, vbl, vso, vspw;
	char phsync, pvsync;
	const char *stereo;
	unsigned pix_clock;
	const char *aspect;

	switch (x[3] & 0xf) {
	case 0:
		aspect = "1:1";
		break;
	case 1:
		aspect = "5:4";
		break;
	case 2:
		aspect = "4:3";
		break;
	case 3:
		aspect = "15:9";
		break;
	case 4:
		aspect = "16:9";
		break;
	case 5:
		aspect = "16:10";
		break;
	case 6:
		aspect = "64:27";
		break;
	case 7:
		aspect = "256:135";
		break;
	default:
		aspect = "undefined";
		break;
	}
	switch ((x[3] >> 5) & 0x3) {
	case 0:
		stereo = ", no 3D stereo";
		break;
	case 1:
		stereo = ", 3D stereo";
		break;
	case 2:
		stereo = ", 3D stereo depends on user action";
		break;
	case 3:
		stereo = ", reserved";
		break;
	}
	printf("      Aspect %s%s%s\n", aspect, x[3] & 0x80 ? ", preferred" : "", stereo);
	pix_clock = 1 + (x[0] + (x[1] << 8) + (x[2] << 16));
	ha = 1 + (x[4] | (x[5] << 8));
	hbl = 1 + (x[6] | (x[7] << 8));
	hso = 1 + (x[8] | ((x[9] & 0x7f) << 8));
	phsync = ((x[9] >> 7) & 0x1) ? '+' : '-';
	hspw = 1 + (x[10] | (x[11] << 8));
	va = 1 + (x[12] | (x[13] << 8));
	vbl = 1 + (x[14] | (x[15] << 8));
	vso = 1 + (x[16] | ((x[17] & 0x7f) << 8));
	vspw = 1 + (x[18] | (x[19] << 8));
	pvsync = ((x[17] >> 7) & 0x1 ) ? '+' : '-';
	
	printf("      Detailed mode: Clock %.3f MHz, %u mm x %u mm\n"
	       "                     %4u %4u %4u %4u (%3u %3u %3d)\n"
	       "                     %4u %4u %4u %4u (%3u %3u %3d)\n"
	       "                     %chsync %cvsync\n"
	       "                     VertFreq: %.3f Hz, HorFreq: %.3f kHz\n",
	       (float)pix_clock/100.0, 0, 0,
	       ha, ha + hso, ha + hso + hspw, ha + hbl, hso, hspw, hbl - hso - hspw,
	       va, va + vso, va + vso + vspw, va + vbl, vso, vspw, vbl - vso - vspw,
	       phsync, pvsync,
	       (pix_clock * 10000.0) / ((ha + hbl) * (va + vbl)),
	       (pix_clock * 10.0) / (ha + hbl)
	      );
}

void parse_displayid_block(edid_state &state, const unsigned char *x)
{
	const unsigned char *orig = x;
	unsigned version = x[1];
	unsigned length = x[2];
	unsigned ext_count = x[4];
	unsigned i;

	printf("Length %u, version %u.%u, extension count %u\n",
	       length, version >> 4, version & 0xf, ext_count);

	unsigned offset = 5;
	while (length > 0) {
		unsigned tag = x[offset];
		unsigned len = x[offset + 2];

		if (len == 0)
			break;
		switch (tag) {
		case 0:
			printf("  Product ID Block\n");
			hex_block("    ", x + offset + 3, len - 3);
			break;
		case 1:
			printf("  Display Parameters Block\n");
			hex_block("    ", x + offset + 3, len - 3);
			break;
		case 2:
			printf("  Color Characteristics Block\n");
			hex_block("    ", x + offset + 3, len - 3);
			break;
		case 3: {
			printf("  Type 1 Detailed Timings Block\n");
			for (i = 0; i < len / 20; i++) {
				parse_displayid_detailed_timing(&x[offset + 3 + (i * 20)]);
			}
			break;
		}
		case 4:
			printf("  Type 2 Detailed Timings Block\n");
			hex_block("    ", x + offset + 3, len - 3);
			break;
		case 5:
			printf("  Type 3 Short Timings Block\n");
			hex_block("    ", x + offset + 3, len - 3);
			break;
		case 6:
			printf("  Type 4 DMT Timings Block\n");
			hex_block("    ", x + offset + 3, len - 3);
			break;
		case 7:
			printf("  Type 1 VESA DMT Timings Block\n");
			for (i = 0; i < min(len, 10) * 8; i++)
				if (x[offset + 3 + i / 8] & (1 << (i % 8)))
					print_timings(state, "    ", find_dmt_id(i + 1), "DMT");
			break;
		case 8:
			printf("  CTA Timings Block\n");
			hex_block("    ", x + offset + 3, len - 3);
			break;
		case 9:
			printf("  Video Timing Range Block\n");
			hex_block("    ", x + offset + 3, len - 3);
			break;
		case 0xa:
			printf("  Product Serial Number Block\n");
			hex_block("    ", x + offset + 3, len - 3);
			break;
		case 0xb:
			printf("  GP ASCII String Block\n");
			hex_block("    ", x + offset + 3, len - 3);
			break;
		case 0xc:
			printf("  Display Device Data Block\n");
			hex_block("    ", x + offset + 3, len - 3);
			break;
		case 0xd:
			printf("  Interface Power Sequencing Block\n");
			hex_block("    ", x + offset + 3, len - 3);
			break;
		case 0xe:
			printf("  Transfer Characteristics Block\n");
			hex_block("    ", x + offset + 3, len - 3);
			break;
		case 0xf:
			printf("  Display Interface Block\n");
			hex_block("    ", x + offset + 3, len - 3);
			break;
		case 0x10:
			printf("  Stereo Display Interface Block\n");
			hex_block("    ", x + offset + 3, len - 3);
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

			printf("  Tiled Display Topology Block\n");
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
			printf("  Unknown DisplayID Data Block 0x%x\n", tag);
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
	state.cur_block = "DisplayID";
	do_checksum("  ", orig+1, orig[2] + 5);
}
