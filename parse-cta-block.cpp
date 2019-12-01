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

struct value {
	unsigned value;
	const char *description;
};

struct field {
	const char *name;
	unsigned start, end;
	const struct value *values;
	unsigned n_values;
};

#define DEFINE_FIELD(n, var, s, e, ...)				\
static const struct value var##_values[] =  {			\
	__VA_ARGS__						\
};								\
static const struct field var = {				\
	.name = n,						\
	.start = s,		        			\
	.end = e,						\
	.values = var##_values,	        			\
	.n_values = ARRAY_SIZE(var##_values),			\
}

static void decode_value(const struct field *field, unsigned val,
			 const char *prefix)
{
	const struct value *v = NULL;
	unsigned i;

	for (i = 0; i < field->n_values; i++) {
		v = &field->values[i];

		if (v->value == val)
			break;
	}

	if (i == field->n_values) {
		printf("%s%s: %u\n", prefix, field->name, val);
		return;
	}

	printf("%s%s: %s (%u)\n", prefix, field->name, v->description, val);
}

static void _decode(const struct field **fields, unsigned n_fields,
		    unsigned data, const char *prefix)
{
	unsigned i;

	for (i = 0; i < n_fields; i++) {
		const struct field *f = fields[i];
		unsigned field_length = f->end - f->start + 1;
		unsigned val;

		if (field_length == 32)
			val = data;
		else
			val = (data >> f->start) & ((1 << field_length) - 1);

		decode_value(f, val, prefix);
	}
}

#define decode(fields, data, prefix)    \
	_decode(fields, ARRAY_SIZE(fields), data, prefix)

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

static const struct timings edid_cta_modes1[] = {
	/* VIC 1 */
	{ 640, 480, 60, 4, 3, 31469, 25175 },
	{ 720, 480, 60, 4, 3, 31469, 27000 },
	{ 720, 480, 60, 16, 9, 31469, 27000 },
	{ 1280, 720, 60, 16, 9, 45000, 74250 },
	{ 1920, 1080, 60, 16, 9, 33750, 74250, 0, 1 },
	{ 1440, 480, 60, 4, 3, 15734, 27000, 0, 1 },
	{ 1440, 480, 60, 16, 9, 15734, 27000, 0, 1 },
	{ 1440, 240, 60, 4, 3, 15734, 27000 },
	{ 1440, 240, 60, 16, 9, 15734, 27000 },
	{ 2880, 480, 60, 4, 3, 15734, 54000, 0, 1 },
	/* VIC 11 */
	{ 2880, 480, 60, 16, 9, 15734, 54000, 0, 1 },
	{ 2880, 240, 60, 4, 3, 15734, 54000 },
	{ 2880, 240, 60, 16, 9, 15734, 54000 },
	{ 1440, 480, 60, 4, 3, 31469, 54000 },
	{ 1440, 480, 60, 16, 9, 31469, 54000 },
	{ 1920, 1080, 60, 16, 9, 67500, 148500 },
	{ 720, 576, 50, 4, 3, 31250, 27000 },
	{ 720, 576, 50, 16, 9, 31250, 27000 },
	{ 1280, 720, 50, 16, 9, 37500, 74250 },
	{ 1920, 1080, 50, 16, 9, 28125, 74250, 0, 1 },
	/* VIC 21 */
	{ 1440, 576, 50, 4, 3, 15625, 27000, 0, 1 },
	{ 1440, 576, 50, 16, 9, 15625, 27000, 0, 1 },
	{ 1440, 288, 50, 4, 3, 15625, 27000 },
	{ 1440, 288, 50, 16, 9, 15625, 27000 },
	{ 2880, 576, 50, 4, 3, 15625, 54000, 0, 1 },
	{ 2880, 576, 50, 16, 9, 15625, 54000, 0, 1 },
	{ 2880, 288, 50, 4, 3, 15625, 54000 },
	{ 2880, 288, 50, 16, 9, 15625, 54000 },
	{ 1440, 576, 50, 4, 3, 31250, 54000 },
	{ 1440, 576, 50, 16, 9, 31250, 54000 },
	/* VIC 31 */
	{ 1920, 1080, 50, 16, 9, 56250, 148500 },
	{ 1920, 1080, 24, 16, 9, 27000, 74250 },
	{ 1920, 1080, 25, 16, 9, 28125, 74250 },
	{ 1920, 1080, 30, 16, 9, 33750, 74250 },
	{ 2880, 480, 60, 4, 3, 31469, 108000 },
	{ 2880, 480, 60, 16, 9, 31469, 108000 },
	{ 2880, 576, 50, 4, 3, 31250, 108000 },
	{ 2880, 576, 50, 16, 9, 31250, 108000 },
	{ 1920, 1080, 50, 16, 9, 31250, 72000, 0, 1 },
	{ 1920, 1080, 100, 16, 9, 56250, 148500, 0, 1 },
	/* VIC 41 */
	{ 1280, 720, 100, 16, 9, 75000, 148500 },
	{ 720, 576, 100, 4, 3, 62500, 54000 },
	{ 720, 576, 100, 16, 9, 62500, 54000 },
	{ 1440, 576, 100, 4, 3, 31250, 54000, 0, 1 },
	{ 1440, 576, 100, 16, 9, 31250, 54000, 0, 1 },
	{ 1920, 1080, 120, 16, 9, 67500, 148500, 0, 1 },
	{ 1280, 720, 120, 16, 9, 90000, 148500 },
	{ 720, 480, 120, 4, 3, 62937, 54000 },
	{ 720, 480, 120, 16, 9, 62937, 54000 },
	{ 1440, 480, 120, 4, 3, 31469, 54000, 0, 1 },
	/* VIC 51 */
	{ 1440, 480, 120, 16, 9, 31469, 54000, 0, 1 },
	{ 720, 576, 200, 4, 3, 125000, 108000 },
	{ 720, 576, 200, 16, 9, 125000, 108000 },
	{ 1440, 576, 200, 4, 3, 62500, 108000, 0, 1 },
	{ 1440, 576, 200, 16, 9, 62500, 108000, 0, 1 },
	{ 720, 480, 240, 4, 3, 125874, 108000 },
	{ 720, 480, 240, 16, 9, 125874, 108000 },
	{ 1440, 480, 240, 4, 3, 62937, 108000, 0, 1 },
	{ 1440, 480, 240, 16, 9, 62937, 108000, 0, 1 },
	{ 1280, 720, 24, 16, 9, 18000, 59400 },
	/* VIC 61 */
	{ 1280, 720, 25, 16, 9, 18750, 74250 },
	{ 1280, 720, 30, 16, 9, 22500, 74250 },
	{ 1920, 1080, 120, 16, 9, 135000, 297000 },
	{ 1920, 1080, 100, 16, 9, 112500, 297000 },
	{ 1280, 720, 24, 64, 27, 18000, 59400 },
	{ 1280, 720, 25, 64, 27, 18750, 74250 },
	{ 1280, 720, 30, 64, 27, 22500, 74250 },
	{ 1280, 720, 50, 64, 27, 37500, 74250 },
	{ 1280, 720, 60, 64, 27, 45000, 74250 },
	{ 1280, 720, 100, 64, 27, 75000, 148500 },
	/* VIC 71 */
	{ 1280, 720, 120, 64, 27, 91000, 148500 },
	{ 1920, 1080, 24, 64, 27, 27000, 74250 },
	{ 1920, 1080, 25, 64, 27, 28125, 74250 },
	{ 1920, 1080, 30, 64, 27, 33750, 74250 },
	{ 1920, 1080, 50, 64, 27, 56250, 148500 },
	{ 1920, 1080, 60, 64, 27, 67500, 148500 },
	{ 1920, 1080, 100, 64, 27, 112500, 297000 },
	{ 1920, 1080, 120, 64, 27, 135000, 297000 },
	{ 1680, 720, 24, 64, 27, 18000, 59400 },
	{ 1680, 720, 25, 64, 27, 18750, 59400 },
	/* VIC 81 */
	{ 1680, 720, 30, 64, 27, 22500, 59400 },
	{ 1680, 720, 50, 64, 27, 37500, 82500 },
	{ 1680, 720, 60, 64, 27, 45000, 99000 },
	{ 1680, 720, 100, 64, 27, 82500, 165000 },
	{ 1680, 720, 120, 64, 27, 99000, 198000 },
	{ 2560, 1080, 24, 64, 27, 26400, 99000 },
	{ 2560, 1080, 25, 64, 27, 28125, 90000 },
	{ 2560, 1080, 30, 64, 27, 33750, 118800 },
	{ 2560, 1080, 50, 64, 27, 56250, 185625 },
	{ 2560, 1080, 60, 64, 27, 66000, 198000 },
	/* VIC 91 */
	{ 2560, 1080, 100, 64, 27, 125000, 371250 },
	{ 2560, 1080, 120, 64, 27, 150000, 495000 },
	{ 3840, 2160, 24, 16, 9, 54000, 297000 },
	{ 3840, 2160, 25, 16, 9, 56250, 297000 },
	{ 3840, 2160, 30, 16, 9, 67500, 297000 },
	{ 3840, 2160, 50, 16, 9, 112500, 594000 },
	{ 3840, 2160, 60, 16, 9, 135000, 594000 },
	{ 4096, 2160, 24, 256, 135, 54000, 297000 },
	{ 4096, 2160, 25, 256, 135, 56250, 297000 },
	{ 4096, 2160, 30, 256, 135, 67500, 297000 },
	/* VIC 101 */
	{ 4096, 2160, 50, 256, 135, 112500, 594000 },
	{ 4096, 2160, 60, 256, 135, 135000, 594000 },
	{ 3840, 2160, 24, 64, 27, 54000, 297000 },
	{ 3840, 2160, 25, 64, 27, 56250, 297000 },
	{ 3840, 2160, 30, 64, 27, 67500, 297000 },
	{ 3840, 2160, 50, 64, 27, 112500, 594000 },
	{ 3840, 2160, 60, 64, 27, 135000, 594000 },
	{ 1280, 720, 48, 16, 9, 36000, 90000 },
	{ 1280, 720, 48, 64, 27, 36000, 90000 },
	{ 1680, 720, 48, 64, 27, 36000, 99000 },
	/* VIC 111 */
	{ 1920, 1080, 48, 16, 9, 54000, 148500 },
	{ 1920, 1080, 48, 64, 27, 54000, 148500 },
	{ 2560, 1080, 48, 64, 27, 52800, 198000 },
	{ 3840, 2160, 48, 16, 9, 108000, 594000 },
	{ 4096, 2160, 48, 256, 135, 108000, 594000 },
	{ 3840, 2160, 48, 64, 27, 108000, 594000 },
	{ 3840, 2160, 100, 16, 9, 225000, 1188000 },
	{ 3840, 2160, 120, 16, 9, 270000, 1188000 },
	{ 3840, 2160, 100, 64, 27, 225000, 1188000 },
	{ 3840, 2160, 120, 64, 27, 270000, 1188000 },
	/* VIC 121 */
	{ 5120, 2160, 24, 64, 27, 52800, 396000 },
	{ 5120, 2160, 25, 64, 27, 55000, 396000 },
	{ 5120, 2160, 30, 64, 27, 66000, 396000 },
	{ 5120, 2160, 48, 64, 27, 118800, 742500 },
	{ 5120, 2160, 50, 64, 27, 112500, 742500 },
	{ 5120, 2160, 60, 64, 27, 135000, 742500 },
	{ 5120, 2160, 100, 64, 27, 225000, 1485000 },
};

static const struct timings edid_cta_modes2[] = {
	/* VIC 193 */
	{ 5120, 2160, 120, 64, 27, 270000, 1485000 },
	{ 7680, 4320, 24, 16, 9, 108000, 1188000 },
	{ 7680, 4320, 25, 16, 9, 110000, 1188000 },
	{ 7680, 4320, 30, 16, 9, 132000, 1188000 },
	{ 7680, 4320, 48, 16, 9, 216000, 2376000 },
	{ 7680, 4320, 50, 16, 9, 220000, 2376000 },
	{ 7680, 4320, 60, 16, 9, 264000, 2376000 },
	{ 7680, 4320, 100, 16, 9, 450000, 4752000 },
	/* VIC 201 */
	{ 7680, 4320, 120, 16, 9, 540000, 4752000 },
	{ 7680, 4320, 24, 64, 27, 108000, 1188000 },
	{ 7680, 4320, 25, 64, 27, 110000, 1188000 },
	{ 7680, 4320, 30, 64, 27, 132000, 1188000 },
	{ 7680, 4320, 48, 64, 27, 216000, 2376000 },
	{ 7680, 4320, 50, 64, 27, 220000, 2376000 },
	{ 7680, 4320, 60, 64, 27, 264000, 2376000 },
	{ 7680, 4320, 100, 64, 27, 450000, 4752000 },
	{ 7680, 4320, 120, 64, 27, 540000, 4752000 },
	{ 10240, 4320, 24, 64, 27, 118800, 1485000 },
	/* VIC 211 */
	{ 10240, 4320, 25, 64, 27, 110000, 1485000 },
	{ 10240, 4320, 30, 64, 27, 135000, 1485000 },
	{ 10240, 4320, 48, 64, 27, 237600, 2970000 },
	{ 10240, 4320, 50, 64, 27, 220000, 2970000 },
	{ 10240, 4320, 60, 64, 27, 270000, 2970000 },
	{ 10240, 4320, 100, 64, 27, 450000, 5940000 },
	{ 10240, 4320, 120, 64, 27, 540000, 5940000 },
	{ 4096, 2160, 100, 256, 135, 225000, 1188000 },
	{ 4096, 2160, 120, 256, 135, 270000, 1188000 },
};

static const struct timings *vic_to_mode(unsigned char vic)
{
	if (vic > 0 && vic <= ARRAY_SIZE(edid_cta_modes1))
		return edid_cta_modes1 + vic - 1;
	if (vic >= 193 && vic <= ARRAY_SIZE(edid_cta_modes2) + 193)
		return edid_cta_modes2 + vic - 193;
	return NULL;
}

void edid_state::cta_svd(const unsigned char *x, unsigned n, int for_ycbcr420)
{
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
		if (!for_ycbcr420)
			svds.push_back(vic);

		if (vic == 1 && !for_ycbcr420)
			has_cta861_vic_1 = 1;
	}
}

void edid_state::cta_y420cmdb(const unsigned char *x, unsigned length)
{
	unsigned i;

	for (i = 0; i < length; i++) {
		unsigned char v = x[0 + i];
		unsigned j;

		for (j = 0; j < 8; j++) {
			if (!(v & (1 << j)))
				continue;

			unsigned idx = i * 8 + j;

			printf("    VDB SVD Index %u", idx);

			if (idx < svds.size()) {
				unsigned char vic = svds[idx];
				const struct timings *t = vic_to_mode(vic);

				if (t)
					print_timings(": ", t, "");
				else
					printf("\n");
			} else {
				printf("\n");
			}
			y420cmdb_max_idx = idx;
		}
	}
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

static const struct timings edid_hdmi_modes[] = {
	{ 3840, 2160, 30, 16, 9, 67500, 297000 },
	{ 3840, 2160, 25, 16, 9, 56250, 297000 },
	{ 3840, 2160, 24, 16, 9, 54000, 297000 },
	{ 4096, 2160, 24, 256, 135, 54000, 297000 },
};

void edid_state::cta_hdmi_block(const unsigned char *x, unsigned length)
{
	unsigned mask = 0, formats = 0;
	unsigned len_vic, len_3d;
	unsigned b = 0;

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

	/* XXX the walk here is really ugly, and needs to be length-checked */
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

	if (x[7] & 0x80) {
		printf("    Video latency: %u\n", x[8 + b]);
		printf("    Audio latency: %u\n", x[9 + b]);
		b += 2;

		if (x[7] & 0x40) {
			printf("    Interlaced video latency: %u\n", x[8 + b]);
			printf("    Interlaced audio latency: %u\n", x[9 + b]);
			b += 2;
		}
	}

	if (!(x[7] & 0x20))
		return;

	printf("    Extended HDMI video details:\n");
	if (x[8 + b] & 0x80)
		printf("      3D present\n");
	if ((x[8 + b] & 0x60) == 0x20) {
		printf("      All advertised VICs are 3D-capable\n");
		formats = 1;
	}
	if ((x[8 + b] & 0x60) == 0x40) {
		printf("      3D-capable-VIC mask present\n");
		formats = 1;
		mask = 1;
	}
	switch (x[8 + b] & 0x18) {
	case 0x00: break;
	case 0x08:
		   printf("      Base EDID image size is aspect ratio\n");
		   break;
	case 0x10:
		   printf("      Base EDID image size is in units of 1cm\n");
		   break;
	case 0x18:
		   printf("      Base EDID image size is in units of 5cm\n");
		   break;
	}
	len_vic = (x[9 + b] & 0xe0) >> 5;
	len_3d = (x[9 + b] & 0x1f) >> 0;
	b += 2;

	if (len_vic) {
		unsigned i;

		printf("      HDMI VICs:\n");
		for (i = 0; i < len_vic; i++) {
			unsigned char vic = x[8 + b + i];
			const struct timings *t;

			if (vic && vic <= ARRAY_SIZE(edid_hdmi_modes)) {
				std::string suffix = "HDMI VIC " + std::to_string(vic);
				supported_hdmi_vic_codes |= 1 << (vic - 1);
				t = &edid_hdmi_modes[vic - 1];
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
			if (x[8 + b] & 0x80)
				printf("      3D: Side-by-side (half, quincunx)\n");
			if (x[8 + b] & 0x01)
				printf("      3D: Side-by-side (half, horizontal)\n");
			/* 3D_Structure_ALL_7..0 */
			if (x[9 + b] & 0x40)
				printf("      3D: Top-and-bottom\n");
			if (x[9 + b] & 0x20)
				printf("      3D: L + depth + gfx + gfx-depth\n");
			if (x[9 + b] & 0x10)
				printf("      3D: L + depth\n");
			if (x[9 + b] & 0x08)
				printf("      3D: Side-by-side (full)\n");
			if (x[9 + b] & 0x04)
				printf("      3D: Line-alternative\n");
			if (x[9 + b] & 0x02)
				printf("      3D: Field-alternative\n");
			if (x[9 + b] & 0x01)
				printf("      3D: Frame-packing\n");
			b += 2;
			len_3d -= 2;
		}
		if (mask) {
			unsigned i;

			printf("      3D VIC indices:");
			/* worst bit ordering ever */
			for (i = 0; i < 8; i++)
				if (x[9 + b] & (1 << i))
					printf(" %u", i);
			for (i = 0; i < 8; i++)
				if (x[8 + b] & (1 << i))
					printf(" %u", i + 8);
			printf("\n");
			b += 2;
			len_3d -= 2;
		}

		/*
		 * list of nibbles:
		 * 2D_VIC_Order_X
		 * 3D_Structure_X
		 * (optionally: 3D_Detail_X and reserved)
		 */
		if (len_3d > 0) {
			unsigned end = b + len_3d;

			while (b < end) {
				printf("      VIC index %u supports ", x[8 + b] >> 4);
				switch (x[8 + b] & 0x0f) {
				case 0: printf("frame packing"); break;
				case 6: printf("top-and-bottom"); break;
				case 8:
					if ((x[9 + b] >> 4) == 1) {
						printf("side-by-side (half, horizontal)");
						break;
					}
				default: printf("unknown");
				}
				printf("\n");

				if ((x[8 + b] & 0x0f) > 7) {
					/* Optional 3D_Detail_X and reserved */
					b++;
				}
				b++;
			}
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
	printf("    Application Version: %u\n", x[0]);
}

DEFINE_FIELD("YCbCr quantization", YCbCr_quantization, 7, 7,
	     { 0, "No Data" },
	     { 1, "Selectable (via AVI YQ)" });
DEFINE_FIELD("RGB quantization", RGB_quantization, 6, 6,
	     { 0, "No Data" },
	     { 1, "Selectable (via AVI Q)" });
DEFINE_FIELD("PT scan behaviour", PT_scan, 4, 5,
	     { 0, "No Data" },
	     { 1, "Always Overscannned" },
	     { 2, "Always Underscanned" },
	     { 3, "Support both over- and underscan" });
DEFINE_FIELD("IT scan behaviour", IT_scan, 2, 3,
	     { 0, "IT video formats not supported" },
	     { 1, "Always Overscannned" },
	     { 2, "Always Underscanned" },
	     { 3, "Support both over- and underscan" });
DEFINE_FIELD("CE scan behaviour", CE_scan, 0, 1,
	     { 0, "CE video formats not supported" },
	     { 1, "Always Overscannned" },
	     { 2, "Always Underscanned" },
	     { 3, "Support both over- and underscan" });

static const struct field *vcdb_fields[] = {
	&YCbCr_quantization,
	&RGB_quantization,
	&PT_scan,
	&IT_scan,
	&CE_scan,
};

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

static float decode_uchar_as_float(unsigned char x)
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
		printf("    DisplayX: %.3f * Xmax\n", decode_uchar_as_float(x[7]));
		printf("    DisplayY: %.3f * Ymax\n", decode_uchar_as_float(x[8]));
		printf("    DisplayZ: %.3f * Zmax\n", decode_uchar_as_float(x[9]));
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
			printf("      X: %.3f * Xmax\n", decode_uchar_as_float(x[2]));
			printf("      Y: %.3f * Ymax\n", decode_uchar_as_float(x[3]));
			printf("      Z: %.3f * Zmax\n", decode_uchar_as_float(x[4]));
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

	decode(vcdb_fields, d, "    ");
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

void edid_state::cta_block(const unsigned char *x)
{
	static int last_block_was_hdmi_vsdb;
	static int have_hf_vsdb, have_hf_scdb;
	static int first_block = 1;
	unsigned length = x[0] & 0x1f;
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
		} else {
			printf(" (unknown)\n");
			hex_block("    ", x + 4, length - 3);
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
			if (oui == 0x90848b) {
				data_block = "Vendor-Specific Video Data Block (HDR10+)";
				printf(" (HDR10+)\n");
				cta_hdr10plus(x + 5, length - 4);
			} else {
				printf(" (unknown)\n");
				printf("    ");
				hex_block("    ", x + 5, length - 4);
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
			printf(" Data Block (tag 0x%02x, length %u)\n", x[1], length - 1);
			hex_block("    ", x + 2, length - 1);
			break;
		}
		break;
	default: {
		unsigned tag = (*x & 0xe0) >> 5;
		unsigned length = *x & 0x1f;
		printf("  Unknown CTA tag 0x%02x, length %u\n", tag, length);
		hex_block("    ", x + 1, length);
		break;
	}
	}
	first_block = 0;
	last_block_was_hdmi_vsdb = 0;
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
			if (offset - 4 > 0)
				/* do stuff */ ;
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
			if (!detailed[0] && !detailed[1]) {
				break;
			}
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
