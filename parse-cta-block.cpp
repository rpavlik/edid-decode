// SPDX-License-Identifier: MIT
/*
 * Copyright 2006-2012 Red Hat, Inc.
 * Copyright 2018-2019 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 *
 * Author: Adam Jackson <ajax@nwnk.net>
 * Maintainer: Hans Verkuil <hverkuil-cisco@xs4all.nl>
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "edid-decode.h"

static const struct timings edid_cta_modes1[] = {
	/* VIC 1 */
	{  640,  480,   4,   3,   25175, false, false,   16,  96,  48, false, 10,  2,  33, false },
	{  720,  480,   4,   3,   27000, false, false,   16,  62,  60, false,  9,  6,  30, false },
	{  720,  480,  16,   9,   27000, false, false,   16,  62,  60, false,  9,  6,  30, false },
	{ 1280,  720,  16,   9,   74250, false, false,  110,  40, 220, true,   5,  5,  20, true  },
	{ 1920, 1080,  16,   9,   74250, false, true,    88,  44, 148, true,   2,  5,  15, true  },
	{ 1440,  480,   4,   3,   27000, false, true,    38, 124, 114, false,  4,  3,  15, false },
	{ 1440,  480,  16,   9,   27000, false, true,    38, 124, 114, false,  4,  3,  15, false },
	{ 1440,  240,   4,   3,   27000, false, false,   38, 124, 114, false,  4,  3,  15, false },
	{ 1440,  240,  16,   9,   27000, false, false,   38, 124, 114, false,  4,  3,  15, false },
	{ 2880,  480,   4,   3,   54000, false, true,    76, 248, 228, false,  4,  3,  15, false },
	/* VIC 11 */
	{ 2880,  480,  16,   9,   54000, false, true,    76, 248, 228, false,  4,  3,  15, false },
	{ 2880,  240,   4,   3,   54000, false, false,   76, 248, 228, false,  4,  3,  15, false },
	{ 2880,  240,  16,   9,   54000, false, false,   76, 248, 228, false,  4,  3,  15, false },
	{ 1440,  480,   4,   3,   54000, false, false,   32, 124, 120, false,  9,  6,  30, false },
	{ 1440,  480,  16,   9,   54000, false, false,   32, 124, 120, false,  9,  6,  30, false },
	{ 1920, 1080,  16,   9,  148500, false, false,   88,  44, 148, true,   4,  5,  36, true  },
	{  720,  576,   4,   3,   27000, false, false,   12,  64,  68, false,  5,  5,  39, false },
	{  720,  576,  16,   9,   27000, false, false,   12,  64,  68, false,  5,  5,  39, false },
	{ 1280,  720,  16,   9,   74250, false, false,  440,  40, 220, true,   5,  5,  20, true  },
	{ 1920, 1080,  16,   9,   74250, false, true,   528,  44, 148, true,   2,  5,  15, true  },
	/* VIC 21 */
	{ 1440,  576,   4,   3,   27000, false, true,    24, 126, 138, false,  2,  3,  19, false },
	{ 1440,  576,  16,   9,   27000, false, true,    24, 126, 138, false,  2,  3,  19, false },
	{ 1440,  288,   4,   3,   27000, false, false,   24, 126, 138, false,  2,  3,  19, false },
	{ 1440,  288,  16,   9,   27000, false, false,   24, 126, 138, false,  2,  3,  19, false },
	{ 2880,  576,   4,   3,   54000, false, true,    48, 252, 276, false,  2,  3,  19, false },
	{ 2880,  576,  16,   9,   54000, false, true,    48, 252, 276, false,  2,  3,  19, false },
	{ 2880,  288,   4,   3,   54000, false, false,   48, 252, 276, false,  2,  3,  19, false },
	{ 2880,  288,  16,   9,   54000, false, false,   48, 252, 276, false,  2,  3,  19, false },
	{ 1440,  576,   4,   3,   54000, false, false,   24, 128, 136, false,  5,  5,  39, false },
	{ 1440,  576,  16,   9,   54000, false, false,   24, 128, 136, false,  5,  5,  39, false },
	/* VIC 31 */
	{ 1920, 1080,  16,   9,  148500, false, false,  528,  44, 148, true,   4,  5,  36, true  },
	{ 1920, 1080,  16,   9,   74250, false, false,  638,  44, 148, true,   4,  5,  36, true  },
	{ 1920, 1080,  16,   9,   74250, false, false,  528,  44, 148, true,   4,  5,  36, true  },
	{ 1920, 1080,  16,   9,   74250, false, false,   88,  44, 148, true,   4,  5,  36, true  },
	{ 2880,  480,   4,   3,  108000, false, false,   64, 248, 240, false,  9,  6,  30, false },
	{ 2880,  480,  16,   9,  108000, false, false,   64, 248, 240, false,  9,  6,  30, false },
	{ 2880,  576,   4,   3,  108000, false, false,   48, 256, 272, false,  5,  5,  39, false },
	{ 2880,  576,  16,   9,  108000, false, false,   48, 256, 272, false,  5,  5,  39, false },
	{ 1920, 1080,  16,   9,   72000, false, true,    32, 168, 184, true,  23,  5,  57, false, 0, 0, true },
	{ 1920, 1080,  16,   9,  148500, false, true,   528,  44, 148, true,   2,  5,  15, true  },
	/* VIC 41 */
	{ 1280,  720,  16,   9,  148500, false, false,  440,  40, 220, true,   5,  5,  20, true  },
	{  720,  576,   4,   3,   54000, false, false,   12,  64,  68, false,  5,  5,  39, false },
	{  720,  576,  16,   9,   54000, false, false,   12,  64,  68, false,  5,  5,  39, false },
	{ 1440,  576,   4,   3,   54000, false, true,    24, 126, 138, false,  2,  3,  19, false },
	{ 1440,  576,  16,   9,   54000, false, true,    24, 126, 138, false,  2,  3,  19, false },
	{ 1920, 1080,  16,   9,  148500, false, true,    88,  44, 148, true,   2,  5,  15, true  },
	{ 1280,  720,  16,   9,  148500, false, false,  110,  40, 220, true,   5,  5,  20, true  },
	{  720,  480,   4,   3,   54000, false, false,   16,  62,  60, false,  9,  6,  30, false },
	{  720,  480,  16,   9,   54000, false, false,   16,  62,  60, false,  9,  6,  30, false },
	{ 1440,  480,   4,   3,   54000, false, true,    38, 124, 114, false,  4,  3,  15, false },
	/* VIC 51 */
	{ 1440,  480,  16,   9,   54000, false, true,    38, 124, 114, false,  4,  3,  15, false },
	{  720,  576,   4,   3,  108000, false, false,   12,  64,  68, false,  5,  5,  39, false },
	{  720,  576,  16,   9,  108000, false, false,   12,  64,  68, false,  5,  5,  39, false },
	{ 1440,  576,   4,   3,  108000, false, true,    24, 126, 138, false,  2,  3,  19, false },
	{ 1440,  576,  16,   9,  108000, false, true,    24, 126, 138, false,  2,  3,  19, false },
	{  720,  480,   4,   3,  108000, false, false,   16,  62,  60, false,  9,  6,  30, false },
	{  720,  480,  16,   9,  108000, false, false,   16,  62,  60, false,  9,  6,  30, false },
	{ 1440,  480,   4,   3,  108000, false, true,    38, 124, 114, false,  4,  3,  15, false },
	{ 1440,  480,  16,   9,  108000, false, true,    38, 124, 114, false,  4,  3,  15, false },
	{ 1280,  720,  16,   9,   59400, false, false, 1760,  40, 220, true,   5,  5,  20, true  },
	/* VIC 61 */
	{ 1280,  720,  16,   9,   74250, false, false, 2420,  40, 220, true,   5,  5,  20, true  },
	{ 1280,  720,  16,   9,   74250, false, false, 1760,  40, 220, true,   5,  5,  20, true  },
	{ 1920, 1080,  16,   9,  297000, false, false,   88,  44, 148, true,   4,  5,  36, true  },
	{ 1920, 1080,  16,   9,  297000, false, false,  528,  44, 148, true,   4,  5,  36, true  },
	{ 1280,  720,  64,  27,   59400, false, false, 1760,  40, 220, true,   5,  5,  20, true  },
	{ 1280,  720,  64,  27,   74250, false, false, 2420,  40, 220, true,   5,  5,  20, true  },
	{ 1280,  720,  64,  27,   74250, false, false, 1760,  40, 220, true,   5,  5,  20, true  },
	{ 1280,  720,  64,  27,   74250, false, false,  440,  40, 220, true,   5,  5,  20, true  },
	{ 1280,  720,  64,  27,   74250, false, false,  110,  40, 220, true,   5,  5,  20, true  },
	{ 1280,  720,  64,  27,  148500, false, false,  440,  40, 220, true,   5,  5,  20, true  },
	/* VIC 71 */
	{ 1280,  720,  64,  27,  148500, false, false,  110,  40, 220, true,   5,  5,  20, true  },
	{ 1920, 1080,  64,  27,   74250, false, false,  638,  44, 148, true,   4,  5,  36, true  },
	{ 1920, 1080,  64,  27,   74250, false, false,  528,  44, 148, true,   4,  5,  36, true  },
	{ 1920, 1080,  64,  27,   74250, false, false,   88,  44, 148, true,   4,  5,  36, true  },
	{ 1920, 1080,  64,  27,  148500, false, false,  528,  44, 148, true,   4,  5,  36, true  },
	{ 1920, 1080,  64,  27,  148500, false, false,   88,  44, 148, true,   4,  5,  36, true  },
	{ 1920, 1080,  64,  27,  297000, false, false,  528,  44, 148, true,   4,  5,  36, true  },
	{ 1920, 1080,  64,  27,  297000, false, false,   88,  44, 148, true,   4,  5,  36, true  },
	{ 1680,  720,  64,  27,   59400, false, false, 1360,  40, 220, true,   5,  5,  20, true  },
	{ 1680,  720,  64,  27,   59400, false, false, 1228,  40, 220, true,   5,  5,  20, true  },
	/* VIC 81 */
	{ 1680,  720,  64,  27,   59400, false, false,  700,  40, 220, true,   5,  5,  20, true  },
	{ 1680,  720,  64,  27,   82500, false, false,  260,  40, 220, true,   5,  5,  20, true  },
	{ 1680,  720,  64,  27,   99000, false, false,  260,  40, 220, true,   5,  5,  20, true  },
	{ 1680,  720,  64,  27,  165000, false, false,   60,  40, 220, true,   5,  5,  95, true  },
	{ 1680,  720,  64,  27,  198000, false, false,   60,  40, 220, true,   5,  5,  95, true  },
	{ 2560, 1080,  64,  27,   99000, false, false,  998,  44, 148, true,   4,  5,  11, true  },
	{ 2560, 1080,  64,  27,   90000, false, false,  448,  44, 148, true,   4,  5,  36, true  },
	{ 2560, 1080,  64,  27,  118800, false, false,  768,  44, 148, true,   4,  5,  36, true  },
	{ 2560, 1080,  64,  27,  185625, false, false,  548,  44, 148, true,   4,  5,  36, true  },
	{ 2560, 1080,  64,  27,  198000, false, false,  248,  44, 148, true,   4,  5,  11, true  },
	/* VIC 91 */
	{ 2560, 1080,  64,  27,  371250, false, false,  218,  44, 148, true,   4,  5, 161, true  },
	{ 2560, 1080,  64,  27,  495000, false, false,  548,  44, 148, true,   4,  5, 161, true  },
	{ 3840, 2160,  16,   9,  297000, false, false, 1276,  88, 296, true,   8, 10,  72, true  },
	{ 3840, 2160,  16,   9,  297000, false, false, 1056,  88, 296, true,   8, 10,  72, true  },
	{ 3840, 2160,  16,   9,  297000, false, false,  176,  88, 296, true,   8, 10,  72, true  },
	{ 3840, 2160,  16,   9,  594000, false, false, 1056,  88, 296, true,   8, 10,  72, true  },
	{ 3840, 2160,  16,   9,  594000, false, false,  176,  88, 296, true,   8, 10,  72, true  },
	{ 4096, 2160, 256, 135,  297000, false, false, 1020,  88, 296, true,   8, 10,  72, true  },
	{ 4096, 2160, 256, 135,  297000, false, false,  968,  88, 128, true,   8, 10,  72, true  },
	{ 4096, 2160, 256, 135,  297000, false, false,   88,  88, 128, true,   8, 10,  72, true  },
	/* VIC 101 */
	{ 4096, 2160, 256, 135,  594000, false, false,  968,  88, 128, true,   8, 10,  72, true  },
	{ 4096, 2160, 256, 135,  594000, false, false,   88,  88, 128, true,   8, 10,  72, true  },
	{ 3840, 2160,  64,  27,  297000, false, false, 1276,  88, 296, true,   8, 10,  72, true  },
	{ 3840, 2160,  64,  27,  297000, false, false, 1056,  88, 296, true,   8, 10,  72, true  },
	{ 3840, 2160,  64,  27,  297000, false, false,  176,  88, 296, true,   8, 10,  72, true  },
	{ 3840, 2160,  64,  27,  594000, false, false, 1056,  88, 296, true,   8, 10,  72, true  },
	{ 3840, 2160,  64,  27,  594000, false, false,  176,  88, 296, true,   8, 10,  72, true  },
	{ 1280,  720,  16,   9,   90000, false, false,  960,  40, 220, true,   5,  5,  20, true  },
	{ 1280,  720,  64,  27,   90000, false, false,  960,  40, 220, true,   5,  5,  20, true  },
	{ 1680,  720,  64,  27,   99000, false, false,  810,  40, 220, true,   5,  5,  20, true  },
	/* VIC 111 */
	{ 1920, 1080,  16,   9,  148500, false, false,  638,  44, 148, true,   4,  5,  36, true  },
	{ 1920, 1080,  64,  27,  148500, false, false,  638,  44, 148, true,   4,  5,  36, true  },
	{ 2560, 1080,  64,  27,  198000, false, false,  998,  44, 148, true,   4,  5,  11, true  },
	{ 3840, 2160,  16,   9,  594000, false, false, 1276,  88, 296, true,   8, 10,  72, true  },
	{ 4096, 2160, 256, 135,  594000, false, false, 1020,  88, 296, true,   8, 10,  72, true  },
	{ 3840, 2160,  64,  27,  594000, false, false, 1276,  88, 296, true,   8, 10,  72, true  },
	{ 3840, 2160,  16,   9, 1188000, false, false, 1056,  88, 296, true,   8, 10,  72, true  },
	{ 3840, 2160,  16,   9, 1188000, false, false,  176,  88, 296, true,   8, 10,  72, true  },
	{ 3840, 2160,  64,  27, 1188000, false, false, 1056,  88, 296, true,   8, 10,  72, true  },
	{ 3840, 2160,  64,  27, 1188000, false, false,  176,  88, 296, true,   8, 10,  72, true  },
	/* VIC 121 */
	{ 5120, 2160,  64,  27,  396000, false, false, 1996,  88, 296, true,   8, 10,  22, true  },
	{ 5120, 2160,  64,  27,  396000, false, false, 1696,  88, 296, true,   8, 10,  22, true  },
	{ 5120, 2160,  64,  27,  396000, false, false,  664,  88, 128, true,   8, 10,  22, true  },
	{ 5120, 2160,  64,  27,  742500, false, false,  746,  88, 296, true,   8, 10, 297, true  },
	{ 5120, 2160,  64,  27,  742500, false, false, 1096,  88, 296, true,   8, 10,  72, true  },
	{ 5120, 2160,  64,  27,  742500, false, false,  164,  88, 128, true,   8, 10,  72, true  },
	{ 5120, 2160,  64,  27, 1485000, false, false, 1096,  88, 296, true,   8, 10,  72, true  },
};

static const struct timings edid_cta_modes2[] = {
	/* VIC 193 */
	{  5120, 2160,  64,  27, 1485000, false, false,  164,  88, 128, true,   8, 10,  72, true  },
	{  7680, 4320,  16,   9, 1188000, false, false, 2552, 176, 592, true,  16, 20, 144, true  },
	{  7680, 4320,  16,   9, 1188000, false, false, 2352, 176, 592, true,  16, 20,  44, true  },
	{  7680, 4320,  16,   9, 1188000, false, false,  552, 176, 592, true,  16, 20,  44, true  },
	{  7680, 4320,  16,   9, 2376000, false, false, 2552, 176, 592, true,  16, 20, 144, true  },
	{  7680, 4320,  16,   9, 2376000, false, false, 2352, 176, 592, true,  16, 20,  44, true  },
	{  7680, 4320,  16,   9, 2376000, false, false,  552, 176, 592, true,  16, 20,  44, true  },
	{  7680, 4320,  16,   9, 4752000, false, false, 2112, 176, 592, true,  16, 20, 144, true  },
	/* VIC 201 */
	{  7680, 4320,  16,   9, 4752000, false, false,  352, 176, 592, true,  16, 20, 144, true  },
	{  7680, 4320,  64,  27, 1188000, false, false, 2552, 176, 592, true,  16, 20, 144, true  },
	{  7680, 4320,  64,  27, 1188000, false, false, 2352, 176, 592, true,  16, 20,  44, true  },
	{  7680, 4320,  64,  27, 1188000, false, false,  552, 176, 592, true,  16, 20,  44, true  },
	{  7680, 4320,  64,  27, 2376000, false, false, 2552, 176, 592, true,  16, 20, 144, true  },
	{  7680, 4320,  64,  27, 2376000, false, false, 2352, 176, 592, true,  16, 20,  44, true  },
	{  7680, 4320,  64,  27, 2376000, false, false,  552, 176, 592, true,  16, 20,  44, true  },
	{  7680, 4320,  64,  27, 4752000, false, false, 2112, 176, 592, true,  16, 20, 144, true  },
	{  7680, 4320,  64,  27, 4752000, false, false,  352, 176, 592, true,  16, 20, 144, true  },
	{ 10240, 4320,  64,  27, 1485000, false, false, 1492, 176, 592, true,  16, 20, 594, true  },
	/* VIC 211 */
	{ 10240, 4320,  64,  27, 1485000, false, false, 2492, 176, 592, true,  16, 20,  44, true  },
	{ 10240, 4320,  64,  27, 1485000, false, false,  288, 176, 296, true,  16, 20, 144, true  },
	{ 10240, 4320,  64,  27, 2970000, false, false, 1492, 176, 592, true,  16, 20, 594, true  },
	{ 10240, 4320,  64,  27, 2970000, false, false, 2492, 176, 592, true,  16, 20,  44, true  },
	{ 10240, 4320,  64,  27, 2970000, false, false,  288, 176, 296, true,  16, 20, 144, true  },
	{ 10240, 4320,  64,  27, 5940000, false, false, 2192, 176, 592, true,  16, 20, 144, true  },
	{ 10240, 4320,  64,  27, 5940000, false, false,  288, 176, 296, true,  16, 20, 144, true  },
	{  4096, 2160, 256, 135, 1188000, false, false,  800,  88, 296, true,   8, 10,  72, true  },
	{  4096, 2160, 256, 135, 1188000, false, false,   88,  88, 128, true,   8, 10,  72, true  },
};

static const unsigned char edid_hdmi_mode_map[] = { 95, 94, 93, 98 };

static const struct timings *vic_to_mode(unsigned char vic)
{
	if (vic > 0 && vic <= ARRAY_SIZE(edid_cta_modes1))
		return edid_cta_modes1 + vic - 1;
	if (vic >= 193 && vic <= ARRAY_SIZE(edid_cta_modes2) + 193)
		return edid_cta_modes2 + vic - 193;
	return NULL;
}

static std::string audio_ext_format(unsigned char x)
{
	switch (x) {
	case 4: return "MPEG-4 HE AAC";
	case 5: return "MPEG-4 HE AAC v2";
	case 6: return "MPEG-4 AAC LC";
	case 7: return "DRA";
	case 8: return "MPEG-4 HE AAC + MPEG Surround";
	case 10: return "MPEG-4 AAC LC + MPEG Surround";
	case 11: return "MPEG-H 3D Audio";
	case 12: return "AC-4";
	case 13: return "L-PCM 3D Audio";
	default: break;
	}
	fail("Unknown Audio Ext Format 0x%02x\n", x);
	return std::string("Unknown Audio Ext Format (") + utohex(x) + ")";
}

static std::string audio_format(unsigned char x)
{
	switch (x) {
	case 1: return "Linear PCM";
	case 2: return "AC-3";
	case 3: return "MPEG 1 (Layers 1 & 2)";
	case 4: return "MPEG 1 Layer 3 (MP3)";
	case 5: return "MPEG2 (multichannel)";
	case 6: return "AAC";
	case 7: return "DTS";
	case 8: return "ATRAC";
	case 9: return "One Bit Audio";
	case 10: return "Dolby Digital+";
	case 11: return "DTS-HD";
	case 12: return "MAT (MLP)";
	case 13: return "DST";
	case 14: return "WMA Pro";
	default: break;
	}
	fail("Unknown Audio Format 0x%02x\n", x);
	return std::string("Unknown Audio Format (") + utohex(x) + ")";
}

static std::string mpeg_h_3d_audio_level(unsigned char x)
{
	switch (x) {
	case 0: return "Unspecified";
	case 1: return "Level 1";
	case 2: return "Level 2";
	case 3: return "Level 3";
	case 4: return "Level 4";
	case 5: return "Level 5";
	default: break;
	}
	fail("Unknown MPEG-H 3D Audio Level 0x%02x\n", x);
	return std::string("Unknown MPEG-H 3D Audio Level (") + utohex(x) + ")";
}

static void cta_audio_block(const unsigned char *x, unsigned length)
{
	unsigned i, format, ext_format = 0;

	if (length % 3) {
		fail("Broken CTA audio block length %d\n", length);
		return;
	}

	for (i = 0; i < length; i += 3) {
		format = (x[i] & 0x78) >> 3;
		ext_format = (x[i + 2] & 0xf8) >> 3;
		if (format == 0) {
			printf("    Reserved (0x00)\n");
			fail("Audio Format Code 0x00 is reserved\n");
			continue;
		}
		if (format != 15)
			printf("    %s, max channels %u\n", audio_format(format).c_str(),
			       (x[i] & 0x07)+1);
		else if (ext_format == 11)
			printf("    %s, MPEG-H 3D Audio Level: %s\n", audio_ext_format(ext_format).c_str(),
			       mpeg_h_3d_audio_level(x[i] & 0x07).c_str());
		else if (ext_format == 13)
			printf("    %s, max channels %u\n", audio_ext_format(ext_format).c_str(),
			       (((x[i + 1] & 0x80) >> 3) | ((x[i] & 0x80) >> 4) |
				(x[i] & 0x07))+1);
		else
			printf("    %s, max channels %u\n", audio_ext_format(ext_format).c_str(),
			       (x[i] & 0x07)+1);
		printf("      Supported sample rates (kHz):%s%s%s%s%s%s%s\n",
		       (x[i+1] & 0x40) ? " 192" : "",
		       (x[i+1] & 0x20) ? " 176.4" : "",
		       (x[i+1] & 0x10) ? " 96" : "",
		       (x[i+1] & 0x08) ? " 88.2" : "",
		       (x[i+1] & 0x04) ? " 48" : "",
		       (x[i+1] & 0x02) ? " 44.1" : "",
		       (x[i+1] & 0x01) ? " 32" : "");
		if (format == 1 || ext_format == 13) {
			printf("      Supported sample sizes (bits):%s%s%s\n",
			       (x[i+2] & 0x04) ? " 24" : "",
			       (x[i+2] & 0x02) ? " 20" : "",
			       (x[i+2] & 0x01) ? " 16" : "");
		} else if (format <= 8) {
			printf("      Maximum bit rate: %u kb/s\n", x[i+2] * 8);
		} else if (format == 10) {
			// As specified by the "Dolby Audio and Dolby Atmos over HDMI"
			// specification (v1.0).
			if(x[i+2] & 1)
				printf("      Supports Joint Object Coding\n");
			if(x[i+2] & 2)
				printf("      Supports Joint Object Coding with ACMOD28\n");
		} else if (format == 14) {
			printf("      Profile: %u\n", x[i+2] & 7);
		} else if (ext_format == 11 && (x[i+2] & 1)) {
			printf("      Supports MPEG-H 3D Audio Low Complexity Profile\n");
		} else if ((ext_format >= 4 && ext_format <= 6) ||
			   ext_format == 8 || ext_format == 10) {
			printf("      AAC audio frame lengths:%s%s\n",
			       (x[i+2] & 4) ? " 1024_TL" : "",
			       (x[i+2] & 2) ? " 960_TL" : "");
			if (ext_format >= 8 && (x[i+2] & 1))
				printf("      Supports %s signaled MPEG Surround data\n",
				       (x[i+2] & 1) ? "implicitly and explicitly" : "only implicitly");
			if (ext_format == 6 && (x[i+2] & 1))
				printf("      Supports 22.2ch System H\n");
		}
	}
}

void edid_state::cta_svd(const unsigned char *x, unsigned n, int for_ycbcr420)
{
	static unsigned short vics[256][2];
	unsigned i;

	for (i = 0; i < n; i++)  {
		const struct timings *t = NULL;
		unsigned char svd = x[i];
		unsigned char native;
		unsigned char vic;

		if ((svd & 0x7f) == 0)
			continue;

		if ((svd - 1) & 0x40) {
			vic = svd;
			native = 0;
		} else {
			vic = svd & 0x7f;
			native = svd & 0x80;
		}

		t = vic_to_mode(vic);
		if (t) {
			switch (vic) {
			case 95:
				supported_hdmi_vic_vsb_codes |= 1 << 0;
				break;
			case 94:
				supported_hdmi_vic_vsb_codes |= 1 << 1;
				break;
			case 93:
				supported_hdmi_vic_vsb_codes |= 1 << 2;
				break;
			case 98:
				supported_hdmi_vic_vsb_codes |= 1 << 3;
				break;
			}
			char suffix[16];

			sprintf(suffix, "VIC %3u%s", vic, native ? ", native" : "");
			print_timings("    ", t, suffix);
		} else {
			printf("    Unknown (VIC %3u)\n", vic);
			fail("Unknown VIC %u\n", vic);
		}

		if (vic == 1 && !for_ycbcr420)
			has_cta861_vic_1 = 1;
		if (++vics[vic][for_ycbcr420] == 2)
			fail("Duplicate %sVIC %u\n", for_ycbcr420 ? "YCbCr 4:2:0 " : "", vic);
		if (for_ycbcr420 && has_vic[0][vic])
			fail("YCbCr 4:2:0-only VIC %u is also a regular VIC\n", vic);
	}
}

void edid_state::print_vic_index(const char *prefix, unsigned idx, const char *suffix)
{
	if (!suffix)
		suffix = "";
	if (idx < svds[0].size()) {
		unsigned char vic = svds[0][idx];
		const struct timings *t = vic_to_mode(vic);
		char buf[256];

		sprintf(buf, "VIC %3u%s%s", vic, *suffix ? ", " : "", suffix);

		if (t)
			print_timings(prefix, t, buf);
		else 
			printf("%sUnknown (%s)\n", prefix, buf);
	} else {
		// Should not happen!
		printf("%sSVD Index %u is out of range", prefix, idx + 1);
		if (*suffix)
			printf(" (%s)", suffix);
		printf("\n");
	}
}

void edid_state::cta_y420cmdb(const unsigned char *x, unsigned length)
{
	unsigned max_idx = 0;
	unsigned i;

	if (!length) {
		printf("    All VDB SVDs\n");
		return;
	}

	if (memchk(x, length)) {
		printf("    Empty Capability Map\n");
		fail("Empty Capability Map\n");
		return;
	}

	for (i = 0; i < length; i++) {
		unsigned char v = x[i];
		unsigned j;

		for (j = 0; j < 8; j++) {
			if (!(v & (1 << j)))
				continue;

			print_vic_index("    ", i * 8 + j, "");
			max_idx = i * 8 + j;
			if (max_idx < svds[0].size()) {
				unsigned vic = svds[0][max_idx];
				if (has_vic[1][vic])
					fail("VIC %u is also a YCbCr 4:2:0-only VIC\n", vic);
			}
		}
	}
	if (max_idx >= svds[0].size())
		fail("Max index %u > %u (#SVDs)\n",
		     max_idx + 1, svds[0].size());
}

void edid_state::cta_vfpdb(const unsigned char *x, unsigned length)
{
	unsigned i;

	for (i = 0; i < length; i++)  {
		unsigned char svr = x[i];

		if ((svr > 0 && svr < 128) || (svr > 192 && svr < 254)) {
			char suffix[16];
			const struct timings *t;
			unsigned char vic = svr;

			sprintf(suffix, "VIC %3u", vic);

			t = vic_to_mode(vic);
			if (t) {
				print_timings("    ", t, suffix);
			} else {
				printf("    Unknown (VIC %3u)\n", vic);
				fail("Unknown VIC %u\n", vic);
			}

		} else if (svr > 128 && svr < 145) {
			printf("    DTD number %02u\n", svr - 128);
		}
	}
}

static std::string hdmi_latency(unsigned char l, bool is_video)
{
	if (!l)
		return "Unknown";
	if (l == 0xff)
		return is_video ? "Video not supported" : "Audio not supported";
	return std::to_string(1 + 2 * l) + " ms";
}

void edid_state::cta_hdmi_block(const unsigned char *x, unsigned length)
{
	unsigned len_vic, len_3d;

	printf(" (HDMI)\n");
	printf("    Source physical address %u.%u.%u.%u\n", x[3] >> 4, x[3] & 0x0f,
	       x[4] >> 4, x[4] & 0x0f);

	if (length < 6)
		return;

	if (x[5] & 0x80)
		printf("    Supports_AI\n");
	if (x[5] & 0x40)
		printf("    DC_48bit\n");
	if (x[5] & 0x20)
		printf("    DC_36bit\n");
	if (x[5] & 0x10)
		printf("    DC_30bit\n");
	if (x[5] & 0x08)
		printf("    DC_Y444\n");
	/* two reserved bits */
	if (x[5] & 0x01)
		printf("    DVI_Dual\n");

	if (length < 7)
		return;

	printf("    Maximum TMDS clock: %u MHz\n", x[6] * 5);
	if (x[6] * 5 > 340)
		fail("HDMI VSDB Max TMDS rate is > 340\n");

	if (length < 8)
		return;

	if (x[7] & 0x0f) {
		printf("    Supported Content Types:\n");
		if (x[7] & 0x01)
			printf("      Graphics\n");
		if (x[7] & 0x02)
			printf("      Photo\n");
		if (x[7] & 0x04)
			printf("      Cinema\n");
		if (x[7] & 0x08)
			printf("      Game\n");
	}

	unsigned b = 8;
	if (x[7] & 0x80) {
		printf("    Video latency: %s\n", hdmi_latency(x[b], true).c_str());
		printf("    Audio latency: %s\n", hdmi_latency(x[b + 1], false).c_str());
		b += 2;

		if (x[7] & 0x40) {
			printf("    Interlaced video latency: %s\n", hdmi_latency(x[b], true).c_str());
			printf("    Interlaced audio latency: %s\n", hdmi_latency(x[b + 1], false).c_str());
			b += 2;
		}
	}

	if (!(x[7] & 0x20))
		return;

	bool mask = false;
	bool formats = false;

	printf("    Extended HDMI video details:\n");
	if (x[b] & 0x80)
		printf("      3D present\n");
	if ((x[b] & 0x60) == 0x20) {
		printf("      All advertised VICs are 3D-capable\n");
		formats = true;
	}
	if ((x[b] & 0x60) == 0x40) {
		printf("      3D-capable-VIC mask present\n");
		formats = true;
		mask = true;
	}
	switch (x[b] & 0x18) {
	case 0x00: break;
	case 0x08:
		   printf("      Base EDID image size is aspect ratio\n");
		   break;
	case 0x10:
		   printf("      Base EDID image size is in units of 1 cm\n");
		   break;
	case 0x18:
		   printf("      Base EDID image size is in units of 5 cm\n");
		   max_display_width_mm *= 5;
		   max_display_height_mm *= 5;
		   printf("        Recalculated image size: %u cm x %u cm\n",
			  max_display_width_mm / 10, max_display_height_mm / 10);
		   break;
	}
	b++;
	len_vic = (x[b] & 0xe0) >> 5;
	len_3d = (x[b] & 0x1f) >> 0;
	b++;

	if (len_vic) {
		unsigned i;

		printf("      HDMI VICs:\n");
		for (i = 0; i < len_vic; i++) {
			unsigned char vic = x[b + i];
			const struct timings *t;

			if (vic && vic <= ARRAY_SIZE(edid_hdmi_mode_map)) {
				std::string suffix = "HDMI VIC " + std::to_string(vic);
				supported_hdmi_vic_codes |= 1 << (vic - 1);
				t = vic_to_mode(edid_hdmi_mode_map[vic - 1]);
				print_timings("        ", t, suffix.c_str());
			} else {
				printf("         Unknown (HDMI VIC %u)\n", vic);
				fail("Unknown HDMI VIC %u\n", vic);
			}
		}

		b += len_vic;
	}

	if (len_3d) {
		if (formats) {
			/* 3D_Structure_ALL_15..8 */
			if (x[b] & 0x80)
				printf("      3D: Side-by-side (half, quincunx)\n");
			if (x[b] & 0x01)
				printf("      3D: Side-by-side (half, horizontal)\n");
			/* 3D_Structure_ALL_7..0 */
			b++;
			if (x[b] & 0x40)
				printf("      3D: Top-and-bottom\n");
			if (x[b] & 0x20)
				printf("      3D: L + depth + gfx + gfx-depth\n");
			if (x[b] & 0x10)
				printf("      3D: L + depth\n");
			if (x[b] & 0x08)
				printf("      3D: Side-by-side (full)\n");
			if (x[b] & 0x04)
				printf("      3D: Line-alternative\n");
			if (x[b] & 0x02)
				printf("      3D: Field-alternative\n");
			if (x[b] & 0x01)
				printf("      3D: Frame-packing\n");
			b++;
			len_3d -= 2;
		}

		if (mask) {
			int max_idx = -1;
			unsigned i;

			printf("      3D VIC indices that support these capabilities:\n");
			/* worst bit ordering ever */
			for (i = 0; i < 8; i++)
				if (x[b + 1] & (1 << i)) {
					print_vic_index("        ", i, "");
					max_idx = i;
				}
			for (i = 0; i < 8; i++)
				if (x[b] & (1 << i)) {
					print_vic_index("        ", i + 8, "");
					max_idx = i + 8;
				}
			b += 2;
			len_3d -= 2;
			if (max_idx >= (int)svds[0].size())
				fail("HDMI 3D VIC indices max index %d > %u (#SVDs)\n",
				     max_idx + 1, svds[0].size());
		}

		/*
		 * list of nibbles:
		 * 2D_VIC_Order_X
		 * 3D_Structure_X
		 * (optionally: 3D_Detail_X and reserved)
		 */
		if (len_3d > 0) {
			unsigned end = b + len_3d;
		int max_idx = -1;

			printf("      3D VIC indices with specific capabilities:\n");
			while (b < end) {
				unsigned char idx = x[b] >> 4;
				std::string s;

				if (idx > max_idx)
					max_idx = idx;
				switch (x[b] & 0x0f) {
				case 0: s = "frame packing"; break;
				case 1: s = "field alternative"; break;
				case 2: s = "line alternative"; break;
				case 3: s = "side-by-side (full)"; break;
				case 4: s = "L + depth"; break;
				case 5: s = "L + depth + gfx + gfx-depth"; break;
				case 6: s = "top-and-bottom"; break;
				case 8:
					s = "side-by-side";
					switch (x[b + 1] >> 4) {
					case 0x00: break;
					case 0x01: s += ", horizontal"; break;
					case 0x02: case 0x03: case 0x04: case 0x05:
						   s += ", not in use";
						   fail("not-in-use 3D_Detail_X value 0x%02x\n",
							x[b + 1] >> 4);
						   break;
					case 0x06: s += ", all quincunx combinations"; break;
					case 0x07: s += ", quincunx odd/left, odd/right"; break;
					case 0x08: s += ", quincunx odd/left, even/right"; break;
					case 0x09: s += ", quincunx even/left, odd/right"; break;
					case 0x0a: s += ", quincunx even/left, even/right"; break;
					default:
						   s += ", reserved";
						   fail("reserved 3D_Detail_X value 0x%02x\n",
							x[b + 1] >> 4);
						   break;
					}
					break;
				default:
					s = "unknown (";
					s += utohex(x[b] & 0x0f) + ")";
					fail("Unknown 3D_Structure_X value 0x%02x\n", x[b] & 0x0f);
					break;
				}
				print_vic_index("        ", idx, s.c_str());
				if ((x[b] & 0x0f) >= 8)
					b++;
				b++;
			}
			if (max_idx >= (int)svds[0].size())
				fail("HDMI 2D VIC indices max index %d > %u (#SVDs)\n",
				     max_idx + 1, svds[0].size());
		}
	}
}

static const char *max_frl_rates[] = {
	"Not Supported",
	"3 Gbps per lane on 3 lanes",
	"3 and 6 Gbps per lane on 3 lanes",
	"3 and 6 Gbps per lane on 3 lanes, 6 Gbps on 4 lanes",
	"3 and 6 Gbps per lane on 3 lanes, 6 and 8 Gbps on 4 lanes",
	"3 and 6 Gbps per lane on 3 lanes, 6, 8 and 10 Gbps on 4 lanes",
	"3 and 6 Gbps per lane on 3 lanes, 6, 8, 10 and 12 Gbps on 4 lanes",
};

static const char *dsc_max_slices[] = {
	"Not Supported",
	"up to 1 slice and up to (340 MHz/Ksliceadjust) pixel clock per slice",
	"up to 2 slices and up to (340 MHz/Ksliceadjust) pixel clock per slice",
	"up to 4 slices and up to (340 MHz/Ksliceadjust) pixel clock per slice",
	"up to 8 slices and up to (340 MHz/Ksliceadjust) pixel clock per slice",
	"up to 8 slices and up to (400 MHz/Ksliceadjust) pixel clock per slice",
	"up to 12 slices and up to (400 MHz/Ksliceadjust) pixel clock per slice",
	"up to 16 slices and up to (400 MHz/Ksliceadjust) pixel clock per slice",
};

static void cta_hf_eeodb(const unsigned char *x, unsigned length)
{
	printf("    EDID Extension Block Count: %u\n", x[0]);
	if (length != 1 || x[0] == 0)
		fail("Block is too long or reports a 0 block count\n");
}

static void cta_hf_scdb(const unsigned char *x, unsigned length)
{
	unsigned rate = x[1] * 5;

	printf("    Version: %u\n", x[0]);
	if (rate) {
		printf("    Maximum TMDS Character Rate: %u MHz\n", rate);
		if ((rate && rate <= 340) || rate > 600)
			fail("Max TMDS rate is > 0 and <= 340 or > 600\n");
	}
	if (x[2] & 0x80)
		printf("    SCDC Present\n");
	if (x[2] & 0x40)
		printf("    SCDC Read Request Capable\n");
	if (x[2] & 0x10)
		printf("    Supports Color Content Bits Per Component Indication\n");
	if (x[2] & 0x08)
		printf("    Supports scrambling for <= 340 Mcsc\n");
	if (x[2] & 0x04)
		printf("    Supports 3D Independent View signaling\n");
	if (x[2] & 0x02)
		printf("    Supports 3D Dual View signaling\n");
	if (x[2] & 0x01)
		printf("    Supports 3D OSD Disparity signaling\n");
	if (x[3] & 0xf0) {
		unsigned max_frl_rate = x[3] >> 4;

		printf("    Max Fixed Rate Link: ");
		if (max_frl_rate < ARRAY_SIZE(max_frl_rates)) {
			printf("%s\n", max_frl_rates[max_frl_rate]);
		} else {
			printf("Unknown (0x%02x)\n", max_frl_rate);
			fail("Unknown Max Fixed Rate Link (0x%02x)\n", max_frl_rate);
		}
		if (max_frl_rate == 1 && rate < 300)
			fail("Max Fixed Rate Link is 1, but Max TMDS rate < 300\n");
		else if (max_frl_rate >= 2 && rate < 600)
			fail("Max Fixed Rate Link is >= 2, but Max TMDS rate < 600\n");
	}
	if (x[3] & 0x08)
		printf("    Supports UHD VIC\n");
	if (x[3] & 0x04)
		printf("    Supports 16-bits/component Deep Color 4:2:0 Pixel Encoding\n");
	if (x[3] & 0x02)
		printf("    Supports 12-bits/component Deep Color 4:2:0 Pixel Encoding\n");
	if (x[3] & 0x01)
		printf("    Supports 10-bits/component Deep Color 4:2:0 Pixel Encoding\n");

	if (length <= 7)
		return;

	if (x[4] & 0x20)
		printf("    Supports Mdelta\n");
	if (x[4] & 0x10)
		printf("    Supports media rates below VRRmin (CinemaVRR)\n");
	if (x[4] & 0x08)
		printf("    Supports negative Mvrr values\n");
	if (x[4] & 0x04)
		printf("    Supports Fast Vactive\n");
	if (x[4] & 0x02)
		printf("    Supports Auto Low-Latency Mode\n");
	if (x[4] & 0x01)
		printf("    Supports a FAPA in blanking after first active video line\n");

	if (length <= 8)
		return;

	printf("    VRRmin: %d Hz\n", x[8] & 0x3f);
	printf("    VRRmax: %d Hz\n", (x[8] & 0xc0) << 2 | x[9]);

	if (length <= 10)
		return;

	if (x[7] & 0x80)
		printf("    Supports VESA DSC 1.2a compression\n");
	if (x[7] & 0x40)
		printf("    Supports Compressed Video Transport for 4:2:0 Pixel Encoding\n");
	if (x[7] & 0x08)
		printf("    Supports Compressed Video Transport at any valid 1/16th bit bpp\n");
	if (x[7] & 0x04)
		printf("    Supports 16 bpc Compressed Video Transport\n");
	if (x[7] & 0x02)
		printf("    Supports 12 bpc Compressed Video Transport\n");
	if (x[7] & 0x01)
		printf("    Supports 10 bpc Compressed Video Transport\n");
	if (x[8] & 0xf) {
		unsigned max_slices = x[8] & 0xf;

		printf("    DSC Max Slices: ");
		if (max_slices < ARRAY_SIZE(dsc_max_slices)) {
			printf("%s\n", dsc_max_slices[max_slices]);
		} else {
			printf("Unknown (0x%02x)\n", max_slices);
			fail("Unknown DSC Max Slices (0x%02x)\n", max_slices);
		}
	}
	if (x[8] & 0xf0) {
		unsigned max_frl_rate = x[8] >> 4;

		printf("    DSC Max Fixed Rate Link: ");
		if (max_frl_rate < ARRAY_SIZE(max_frl_rates)) {
			printf("%s\n", max_frl_rates[max_frl_rate]);
		} else {
			printf("Unknown (0x%02x)\n", max_frl_rate);
			fail("Unknown DSC Max Fixed Rate Link (0x%02x)\n", max_frl_rate);
		}
	}
	if (x[9] & 0x3f)
		printf("    Maximum number of bytes in a line of chunks: %u\n",
		       1024 * (1 + (x[9] & 0x3f)));
}

static void cta_hdr10plus(const unsigned char *x, unsigned length)
{
	printf("    Application Version: %u", x[0]);
	hex_block("  ", x + 1, length - 1);
}

static const char *speaker_map[] = {
	"FL/FR - Front Left/Right",
	"LFE1 - Low Frequency Effects 1",
	"FC - Front Center",
	"BL/BR - Back Left/Right",
	"BC - Back Center",
	"FLc/FRc - Front Left/Right of Center",
	"RLC/RRC - Rear Left/Right of Center (Deprecated)",
	"FLw/FRw - Front Left/Right Wide",
	"TpFL/TpFR - Top Front Left/Right",
	"TpC - Top Center",
	"TpFC - Top Front Center",
	"LS/RS - Left/Right Surround",
	"LFE2 - Low Frequency Effects 2",
	"TpBC - Top Back Center",
	"SiL/SiR - Side Left/Right",
	"TpSiL/TpSiR - Top Side Left/Right",
	"TpBL/TpBR - Top Back Left/Right",
	"BtFC - Bottom Front Center",
	"BtFL/BtFR - Bottom Front Left/Right",
	"TpLS/TpRS - Top Left/Right Surround (Deprecated for CTA-861)",
	"LSd/RSd - Left/Right Surround Direct (HDMI only)",
};

static void cta_sadb(const unsigned char *x, unsigned length)
{
	unsigned sad;
	unsigned i;

	if (length < 3)
		return;

	sad = ((x[2] << 16) | (x[1] << 8) | x[0]);

	printf("    Speaker map:\n");

	for (i = 0; i < ARRAY_SIZE(speaker_map); i++) {
		if ((sad >> i) & 1)
			printf("      %s\n", speaker_map[i]);
	}
}

static double decode_uchar_as_double(unsigned char x)
{
	signed char s = (signed char)x;

	return s / 64.0;
}

static void cta_rcdb(const unsigned char *x, unsigned length)
{
	unsigned spm = ((x[3] << 16) | (x[2] << 8) | x[1]);
	unsigned i;

	if (length < 4)
		return;

	if (x[0] & 0x40)
		printf("    Speaker count: %u\n", (x[0] & 0x1f) + 1);

	printf("    Speaker Presence Mask:\n");
	for (i = 0; i < ARRAY_SIZE(speaker_map); i++) {
		if ((spm >> i) & 1)
			printf("      %s\n", speaker_map[i]);
	}
	if ((x[0] & 0x20) && length >= 7) {
		printf("    Xmax: %u dm\n", x[4]);
		printf("    Ymax: %u dm\n", x[5]);
		printf("    Zmax: %u dm\n", x[6]);
	}
	if ((x[0] & 0x80) && length >= 10) {
		printf("    DisplayX: %.3f * Xmax\n", decode_uchar_as_double(x[7]));
		printf("    DisplayY: %.3f * Ymax\n", decode_uchar_as_double(x[8]));
		printf("    DisplayZ: %.3f * Zmax\n", decode_uchar_as_double(x[9]));
	}
}

static const char *speaker_location[] = {
	"FL - Front Left",
	"FR - Front Right",
	"FC - Front Center",
	"LFE1 - Low Frequency Effects 1",
	"BL - Back Left",
	"BR - Back Right",
	"FLC - Front Left of Center",
	"FRC - Front Right of Center",
	"BC - Back Center",
	"LFE2 - Low Frequency Effects 2",
	"SiL - Side Left",
	"SiR - Side Right",
	"TpFL - Top Front Left",
	"TpFR - Top Front Right",
	"TpFC - Top Front Center",
	"TpC - Top Center",
	"TpBL - Top Back Left",
	"TpBR - Top Back Right",
	"TpSiL - Top Side Left",
	"TpSiR - Top Side Right",
	"TpBC - Top Back Center",
	"BtFC - Bottom Front Center",
	"BtFL - Bottom Front Left",
	"BtFR - Bottom Front Right",
	"FLW - Front Left Wide",
	"FRW - Front Right Wide",
	"LS - Left Surround",
	"RS - Right Surround",
};

static void cta_sldb(const unsigned char *x, unsigned length)
{
	while (length >= 2) {
		printf("    Channel: %u (%sactive)\n", x[0] & 0x1f,
		       (x[0] & 0x20) ? "" : "not ");
		if ((x[1] & 0x1f) < ARRAY_SIZE(speaker_location))
			printf("      Speaker: %s\n", speaker_location[x[1] & 0x1f]);
		if (length >= 5 && (x[0] & 0x40)) {
			printf("      X: %.3f * Xmax\n", decode_uchar_as_double(x[2]));
			printf("      Y: %.3f * Ymax\n", decode_uchar_as_double(x[3]));
			printf("      Z: %.3f * Zmax\n", decode_uchar_as_double(x[4]));
			length -= 3;
			x += 3;
		}

		length -= 2;
		x += 2;
	}
}

static void cta_vcdb(const unsigned char *x, unsigned length)
{
	unsigned char d = x[0];

	printf("    YCbCr quantization: %s\n",
	       (d & 0x80) ? "Selectable (via AVI YQ)" : "No Data");
	printf("    RGB quantization: %s\n",
	       (d & 0x40) ? "Selectable (via AVI Q)" : "No Data");
	/*
	 * If this bit is not set then that will result in interoperability
	 * problems (specifically with PCs/laptops) that quite often do not
	 * follow the default rules with respect to RGB Quantization Range
	 * handling.
	 *
	 * The HDMI 2.0 spec recommends that this is set, but it is a good
	 * recommendation in general, not just for HDMI.
	 */
	if (!(d & 0x40))
		warn("Set Selectable RGB Quantization to avoid interop issues.\n");
	/*
	 * HDMI 2.0 recommends that the Selectable YCbCr Quantization bit is set
	 * as well, but in practice this is less of an interop issue.
	 *
	 * I decided to not warn about this (for now).
	 *
	 * if (!(d & 0x80))
	 *	warn("Set Selectable YCbCr Quantization to avoid interop issues.\n");
	 */
	printf("    PT scan behavior: ");
	switch ((d >> 4) & 0x03) {
	case 0: printf("No Data\n"); break;
	case 1: printf("Always Overscanned\n"); break;
	case 2: printf("Always Underscanned\n"); break;
	case 3: printf("Supports both over- and underscan\n"); break;
	}
	printf("    IT scan behavior: ");
	switch ((d >> 6) & 0x03) {
	case 0: printf("IT video formats not supported\n"); break;
	case 1: printf("Always Overscanned\n"); break;
	case 2: printf("Always Underscanned\n"); break;
	case 3: printf("Supports both over- and underscan\n"); break;
	}
	printf("    CE scan behavior: ");
	switch (d & 0x03) {
	case 0: printf("CE video formats not supported\n"); break;
	case 1: printf("Always Overscanned\n"); break;
	case 2: printf("Always Underscanned\n"); break;
	case 3: printf("Supports both over- and underscan\n"); break;
	}
}

static const char *colorimetry_map[] = {
	"xvYCC601",
	"xvYCC709",
	"sYCC601",
	"opYCC601",
	"opRGB",
	"BT2020cYCC",
	"BT2020YCC",
	"BT2020RGB",
};

static void cta_colorimetry_block(const unsigned char *x, unsigned length)
{
	unsigned i;

	if (length >= 2) {
		for (i = 0; i < ARRAY_SIZE(colorimetry_map); i++) {
			if (x[0] & (1 << i))
				printf("    %s\n", colorimetry_map[i]);
		}
		if (x[1] & 0x80)
			printf("    DCI-P3\n");
		if (x[1] & 0x40)
			printf("    ICtCp\n");
	}
}

static const char *eotf_map[] = {
	"Traditional gamma - SDR luminance range",
	"Traditional gamma - HDR luminance range",
	"SMPTE ST2084",
	"Hybrid Log-Gamma",
};

static void cta_hdr_static_metadata_block(const unsigned char *x, unsigned length)
{
	unsigned i;

	if (length >= 2) {
		printf("    Electro optical transfer functions:\n");
		for (i = 0; i < 6; i++) {
			if (x[0] & (1 << i)) {
				if (i < ARRAY_SIZE(eotf_map)) {
					printf("      %s\n", eotf_map[i]);
				} else {
					printf("      Unknown (%u)\n", i);
					fail("Unknown EOTF (%u)\n", i);
				}
			}
		}
		printf("    Supported static metadata descriptors:\n");
		for (i = 0; i < 8; i++) {
			if (x[1] & (1 << i))
				printf("      Static metadata type %u\n", i + 1);
		}
	}

	if (length >= 3)
		printf("    Desired content max luminance: %u (%.3f cd/m^2)\n",
		       x[2], 50.0 * pow(2, x[2] / 32.0));

	if (length >= 4)
		printf("    Desired content max frame-average luminance: %u (%.3f cd/m^2)\n",
		       x[3], 50.0 * pow(2, x[3] / 32.0));

	if (length >= 5)
		printf("    Desired content min luminance: %u (%.3f cd/m^2)\n",
		       x[4], (50.0 * pow(2, x[2] / 32.0)) * pow(x[4] / 255.0, 2) / 100.0);
}

static void cta_hdr_dyn_metadata_block(const unsigned char *x, unsigned length)
{
	while (length >= 3) {
		unsigned type_len = x[0];
		unsigned type = x[1] | (x[2] << 8);

		if (length < type_len + 1)
			return;
		printf("    HDR Dynamic Metadata Type %u\n", type);
		switch (type) {
		case 1:
		case 2:
		case 4:
			if (type_len > 2)
				printf("      Version: %u\n", x[3] & 0xf);
			break;
		default:
			break;
		}
		length -= type_len + 1;
		x += type_len + 1;
	}
}

static void cta_ifdb(const unsigned char *x, unsigned length)
{
	unsigned len_hdr = x[0] >> 5;

	if (length < 2)
		return;
	printf("    VSIFs: %u\n", x[1]);
	if (length < len_hdr + 2)
		return;
	length -= len_hdr + 2;
	x += len_hdr + 2;
	while (length > 0) {
		int payload_len = x[0] >> 5;

		if ((x[0] & 0x1f) == 1 && length >= 4) {
			printf("    InfoFrame Type Code %u IEEE OUI: 0x%02x%02x%02x\n",
			       x[0] & 0x1f, x[3], x[2], x[1]);
			x += 4;
			length -= 4;
		} else {
			printf("    InfoFrame Type Code %u\n", x[0] & 0x1f);
			x++;
			length--;
		}
		x += payload_len;
		length -= payload_len;
	}
}

static void cta_hdmi_audio_block(const unsigned char *x, unsigned length)
{
	unsigned num_descs;

	if (length < 2)
		return;
	if (x[0] & 3)
		printf("    Max Stream Count: %u\n", (x[0] & 3) + 1);
	if (x[0] & 4)
		printf("    Supports MS NonMixed\n");

	num_descs = x[1] & 7;
	if (num_descs == 0)
		return;
	length -= 2;
	x += 2;
	while (length >= 4) {
		if (length > 4) {
			unsigned format = x[0] & 0xf;

			printf("    %s, max channels %u\n", audio_format(format).c_str(),
			       (x[1] & 0x1f)+1);
			printf("      Supported sample rates (kHz):%s%s%s%s%s%s%s\n",
			       (x[2] & 0x40) ? " 192" : "",
			       (x[2] & 0x20) ? " 176.4" : "",
			       (x[2] & 0x10) ? " 96" : "",
			       (x[2] & 0x08) ? " 88.2" : "",
			       (x[2] & 0x04) ? " 48" : "",
			       (x[2] & 0x02) ? " 44.1" : "",
			       (x[2] & 0x01) ? " 32" : "");
			if (format == 1)
				printf("      Supported sample sizes (bits):%s%s%s\n",
				       (x[3] & 0x04) ? " 24" : "",
				       (x[3] & 0x02) ? " 20" : "",
				       (x[3] & 0x01) ? " 16" : "");
		} else {
			unsigned sad = ((x[2] << 16) | (x[1] << 8) | x[0]);
			unsigned i;

			switch (x[3] >> 4) {
			case 1:
				printf("    Speaker Allocation for 10.2 channels:\n");
				break;
			case 2:
				printf("    Speaker Allocation for 22.2 channels:\n");
				break;
			case 3:
				printf("    Speaker Allocation for 30.2 channels:\n");
				break;
			default:
				printf("    Unknown Speaker Allocation (0x%02x)\n", x[3] >> 4);
				return;
			}

			for (i = 0; i < ARRAY_SIZE(speaker_map); i++) {
				if ((sad >> i) & 1)
					printf("      %s\n", speaker_map[i]);
			}
		}
		length -= 4;
		x += 4;
	}
}

static const char *oui_name(unsigned oui)
{
	switch (oui) {
	case 0x00001a: return "AMD";
	case 0x00044b: return "NVIDIA";
	case 0x000c6e: return "ASUS";
	case 0x0010fa: return "Apple";
	case 0x0014b9: return "MSTAR";
	case 0x00d046: return "Dolby";
	case 0x00e047: return "InFocus";
	case 0xca125c: return "Microsoft";
	default: return NULL;
	}
}

void edid_state::cta_block(const unsigned char *x)
{
	static int last_block_was_hdmi_vsdb;
	static int have_hf_vsdb, have_hf_scdb;
	static int first_block = 1;
	unsigned length = x[0] & 0x1f;
	const char *name;
	unsigned oui;

	switch ((x[0] & 0xe0) >> 5) {
	case 0x01:
		data_block = "Audio Data Block";
		printf("  %s\n", data_block.c_str());
		cta_audio_block(x + 1, length);
		break;
	case 0x02:
		data_block = "Video Data Block";
		printf("  %s\n", data_block.c_str());
		cta_svd(x + 1, length, 0);
		break;
	case 0x03:
		oui = (x[3] << 16) + (x[2] << 8) + x[1];
		printf("  Vendor-Specific Data Block, OUI 0x%06x", oui);
		name = oui_name(oui);
		if (oui == 0x000c03) {
			data_block = "Vendor-Specific Data Block (HDMI)";
			cta_hdmi_block(x + 1, length);
			last_block_was_hdmi_vsdb = 1;
			first_block = 0;
			if (edid_minor != 3)
				fail("The HDMI Specification uses EDID 1.3, not 1.%u\n", edid_minor);
			return;
		}
		if (oui == 0xc45dd8) {
			data_block = "Vendor-Specific Data Block (HDMI Forum)";
			if (!last_block_was_hdmi_vsdb)
				fail("HDMI Forum VSDB did not immediately follow the HDMI VSDB\n");
			if (have_hf_scdb || have_hf_vsdb)
				fail("Duplicate HDMI Forum VSDB/SCDB\n");
			printf(" (HDMI Forum)\n");
			cta_hf_scdb(x + 4, length - 3);
			have_hf_vsdb = 1;
		} else if (name) {
			data_block = std::string("Vendor-Specific Data Block (") + name + ")";
			printf(" (%s)\n", name);
			hex_block("    ", x + 4, length - 3);
		} else {
			printf("\n");
			hex_block("    ", x + 4, length - 3);
			data_block.clear();
			warn("Unknown Vendor-Specific Data Block, OUI 0x%06x\n", oui);
		}
		break;
	case 0x04:
		data_block = "Speaker Allocation Data Block";
		printf("  %s\n", data_block.c_str());
		cta_sadb(x + 1, length);
		break;
	case 0x05:
		printf("  VESA DTC Data Block\n");
		hex_block("  ", x + 1, length);
		break;
	case 0x07:
		printf("  Extended tag: ");
		switch (x[1]) {
		case 0x00:
			data_block = "Video Capability Data Block";
			printf("%s\n", data_block.c_str());
			cta_vcdb(x + 2, length - 1);
			break;
		case 0x01:
			oui = (x[4] << 16) + (x[3] << 8) + x[2];
			printf("Vendor-Specific Video Data Block, OUI 0x%06x", oui);
			name = oui_name(oui);
			if (oui == 0x90848b) {
				data_block = "Vendor-Specific Video Data Block (HDR10+)";
				printf(" (HDR10+)\n");
				cta_hdr10plus(x + 5, length - 4);
			} else if (name) {
				data_block = std::string("Vendor-Specific Data Block (") + name + ")";
				printf(" (%s)\n", name);
				hex_block("    ", x + 5, length - 4);
			} else {
				printf("\n");
				hex_block("    ", x + 5, length - 4);
				data_block.clear();
				warn("Unknown Extended Vendor-Specific Data Block, OUI 0x%06x\n", oui);
			}
			break;
		case 0x02:
			printf("VESA Video Display Device Data Block\n");
			hex_block("  ", x + 2, length - 1);
			break;
		case 0x03:
			printf("VESA Video Timing Block Extension\n");
			hex_block("  ", x + 2, length - 1);
			break;
		case 0x04:
			printf("Reserved for HDMI Video Data Block\n");
			hex_block("  ", x + 2, length - 1);
			break;
		case 0x05:
			data_block = "Colorimetry Data Block";
			printf("%s\n", data_block.c_str());
			cta_colorimetry_block(x + 2, length - 1);
			break;
		case 0x06:
			data_block = "HDR Static Metadata Data Block";
			printf("%s\n", data_block.c_str());
			cta_hdr_static_metadata_block(x + 2, length - 1);
			break;
		case 0x07:
			data_block = "HDR Dynamic Metadata Data Block";
			printf("%s\n", data_block.c_str());
			cta_hdr_dyn_metadata_block(x + 2, length - 1);
			break;
		case 0x0d:
			data_block = "Video Format Preference Data Block";
			printf("%s\n", data_block.c_str());
			cta_vfpdb(x + 2, length - 1);
			break;
		case 0x0e:
			data_block = "YCbCr 4:2:0 Video Data Block";
			printf("%s\n", data_block.c_str());
			cta_svd(x + 2, length - 1, 1);
			break;
		case 0x0f:
			data_block = "YCbCr 4:2:0 Capability Map Data Block";
			printf("%s\n", data_block.c_str());
			cta_y420cmdb(x + 2, length - 1);
			break;
		case 0x10:
			printf("Reserved for CTA Miscellaneous Audio Fields\n");
			hex_block("  ", x + 2, length - 1);
			break;
		case 0x11:
			printf("Vendor-Specific Audio Data Block\n");
			hex_block("  ", x + 2, length - 1);
			break;
		case 0x12:
			data_block = "HDMI Audio Data Block";
			printf("%s\n", data_block.c_str());
			cta_hdmi_audio_block(x + 2, length - 1);
			break;
		case 0x13:
			data_block = "Room Configuration Data Block";
			printf("%s\n", data_block.c_str());
			cta_rcdb(x + 2, length - 1);
			break;
		case 0x14:
			data_block = "Speaker Location Data Block";
			printf("%s\n", data_block.c_str());
			cta_sldb(x + 2, length - 1);
			break;
		case 0x20:
			printf("InfoFrame Data Block\n");
			cta_ifdb(x + 2, length - 1);
			break;
		case 0x78:
			data_block = "HDMI Forum EDID Extension Override Data Block";
			printf("%s\n", data_block.c_str());
			cta_hf_eeodb(x + 2, length - 1);
			// This must be the first CTA block
			if (!first_block)
				fail("Block starts at a wrong offset\n");
			break;
		case 0x79:
			data_block = "HDMI Forum Sink Capability Data Block";
			printf("%s\n", data_block.c_str());
			if (!last_block_was_hdmi_vsdb)
				fail("HDMI Forum SCDB did not immediately follow the HDMI VSDB\n");
			if (have_hf_scdb || have_hf_vsdb)
				fail("Duplicate HDMI Forum VSDB/SCDB\n");
			if (x[2] || x[3])
				printf("  Non-zero SCDB reserved fields!\n");
			cta_hf_scdb(x + 4, length - 3);
			have_hf_scdb = 1;
			break;
		default:
			if (x[1] <= 12)
				printf("Unknown CTA Video-Related");
			else if (x[1] <= 31)
				printf("Unknown CTA Audio-Related");
			else if (x[1] >= 120 && x[1] <= 127)
				printf("Unknown CTA HDMI-Related");
			else
				printf("Unknown CTA");
			printf(" Data Block (extended tag 0x%02x, length %u)\n", x[1], length - 1);
			hex_block("    ", x + 2, length - 1);
			data_block.clear();
			warn("Unknown Extended CTA Data Block 0x%02x\n", x[1]);
			break;
		}
		break;
	default: {
		unsigned tag = (*x & 0xe0) >> 5;
		unsigned length = *x & 0x1f;
		printf("  Unknown CTA tag 0x%02x, length %u\n", tag, length);
		hex_block("    ", x + 1, length);
		data_block.clear();
		warn("Unknown CTA Data Block %u\n", tag);
		break;
	}
	}
	first_block = 0;
	last_block_was_hdmi_vsdb = 0;
}

void edid_state::preparse_cta_block(const unsigned char *x)
{
	unsigned version = x[1];
	unsigned offset = x[2];

	if (version < 3)
		return;

	for (unsigned i = 4; i < offset; i += (x[i] & 0x1f) + 1) {
		bool for_ycbcr420 = false;

		switch ((x[i] & 0xe0) >> 5) {
		case 0x07:
			if (x[i + 1] != 0x0e)
				continue;
			for_ycbcr420 = true;
			/* fall-through */
		case 0x02:
			for (unsigned j = 1 + for_ycbcr420; j <= (x[i] & 0x1f); j++) {
				unsigned char vic = x[i + j];

				if ((vic & 0x7f) <= 64)
					vic &= 0x7f;
				svds[for_ycbcr420].push_back(vic);
				has_vic[for_ycbcr420][vic] = true;
			}
			break;
		}
	}
}

void edid_state::parse_cta_block(const unsigned char *x)
{
	unsigned version = x[1];
	unsigned offset = x[2];
	const unsigned char *detailed;

	printf("%s Revision %u\n", block.c_str(), version);

	if (version >= 1) do {
		if (version == 1 && x[3] != 0)
			fail("Non-zero byte 3\n");

		if (offset < 4)
			break;

		if (version < 3) {
			printf("%u 8-byte timing descriptors\n", (offset - 4) / 8);
			if (offset - 4 > 0) {
				/* do stuff */
			}
		}

		if (version >= 2) {    
			if (x[3] & 0x80)
				printf("Underscans PC formats by default\n");
			if (x[3] & 0x40)
				printf("Basic audio support\n");
			if (x[3] & 0x20)
				printf("Supports YCbCr 4:4:4\n");
			if (x[3] & 0x10)
				printf("Supports YCbCr 4:2:2\n");
			printf("%u native detailed modes\n", x[3] & 0x0f);
		}
		if (version == 3) {
			unsigned i;

			printf("%u bytes of CTA data blocks\n", offset - 4);
			for (i = 4; i < offset; i += (x[i] & 0x1f) + 1)
				cta_block(x + i);
		}

		unsigned cnt;
		for (detailed = x + offset, cnt = 1; detailed + 18 < x + 127; detailed += 18, cnt++) {
			if (memchk(detailed, 18))
				break;
			data_block = "Detailed Timings #" + std::to_string(cnt);
			detailed_timings("", detailed);
		}
		if (!memchk(detailed, x + 0x7f - detailed)) {
			data_block = "Padding";
			fail("CTA-861 padding contains non-zero bytes\n");
		}
	} while (0);

	data_block.clear();
	if (has_serial_number && has_serial_string)
		fail("Both the serial number and the serial string are set\n");
	if (!has_cta861_vic_1 && !has_640x480p60_est_timing)
		fail("Required 640x480p60 timings are missing in the established timings"
		     "and the SVD list (VIC 1)\n");
	if ((supported_hdmi_vic_vsb_codes & supported_hdmi_vic_codes) !=
	    supported_hdmi_vic_codes)
		fail("HDMI VIC Codes must have their CTA-861 VIC equivalents in the VSB\n");
}
