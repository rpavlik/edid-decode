// SPDX-License-Identifier: MIT
/*
 * Copyright 2006-2012 Red Hat, Inc.
 * Copyright 2018-2019 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 *
 * Author: Adam Jackson <ajax@nwnk.net>
 * Maintainer: Hans Verkuil <hverkuil-cisco@xs4all.nl>
 */

#include <string.h>

#include "edid-decode.h"

// misc functions

static void check_displayid_datablock_revision(const unsigned char *x)
{
	unsigned char revisionflags = x[1];

	if (revisionflags)
		warn("Unexpected revision and flags (0x%02x != 0)\n", revisionflags);
}

static bool check_displayid_datablock_length(const unsigned char *x,
					     unsigned expectedlenmin = 0,
					     unsigned expectedlenmax = 128 - 2 - 5 - 3,
					     unsigned payloaddumpstart = 0)
{
	unsigned char len = x[2];

	if (expectedlenmin == expectedlenmax && len != expectedlenmax)
		fail("DisplayID payload length is different than expected (%d != %d)\n", len, expectedlenmax);
	else if (len > expectedlenmax)
		fail("DisplayID payload length is greater than expected (%d > %d)\n", len, expectedlenmax);
	else if (len < expectedlenmin)
		fail("DisplayID payload length is less than expected (%d < %d)\n", len, expectedlenmin);
	else
		return true;

	if (len > payloaddumpstart)
		hex_block("    ", x + 3 + payloaddumpstart, len - payloaddumpstart);
	return false;
}

// tag 0x01

static const char *feature_support_flags[] = {
	"De-interlacing",
	"Support ACP, ISRC1, or ISRC2packets",
	"Fixed pixel format",
	"Fixed timing",
	"Power management (DPM)",
	"Audio input override",
	"Separate audio inputs provided",
	"Audio support on video interface"
};

static void print_flag_lines(const char *indent, const char *label,
			     unsigned char flag_byte, const char **flags)
{
	if (flag_byte) {
		printf("%s\n", label);

		for (int i = 0; i < 8; i++)
			if (flag_byte & (1 << i))
				printf("%s%s\n", indent, flags[i]);
	}
}

static void parse_displayid_parameters(const unsigned char *x)
{
	check_displayid_datablock_revision(x);

	if (check_displayid_datablock_length(x, 12, 12)) {
		printf("    Image size: %.1f mm x %.1f mm\n",
		       ((x[4] << 8) + x[3]) / 10.0,
		       ((x[6] << 8) + x[5]) / 10.0);
		printf("    Pixels: %d x %d\n",
		       (x[8] << 8) + x[7], (x[10] << 8) + x[9]);
		print_flag_lines("      ", "    Feature support flags:",
				 x[11], feature_support_flags);

		if (x[12] != 0xff)
			printf("    Gamma: %.2f\n", ((x[12] + 100.0) / 100.0));
		printf("    Aspect ratio: %.2f\n", ((x[13] + 100.0) / 100.0));
		printf("    Dynamic bpc native: %d\n", (x[14] & 0xf) + 1);
		printf("    Dynamic bpc overall: %d\n", ((x[14] >> 4) & 0xf) + 1);
	}
}

// tag 0x03

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
	       t.w, t.w + t.hfp, t.w + t.hfp + t.hsync, t.w + hbl, t.hfp, t.hsync, t.hbp,
	       t.h, t.h + t.vfp, t.h + t.vfp + t.vsync, t.h + vbl, t.vfp, t.vsync, t.vbp,
	       t.pos_pol_hsync ? '+' : '-', t.pos_pol_vsync ? '+' : '-',
	       (t.pixclk_khz * 1000.0) / ((t.w + hbl) * (t.h + vbl)),
	       (double)(t.pixclk_khz) / (t.w + hbl)
	      );
}

// tag 0x0b

static void parse_displayid_gp_string(const unsigned char *x)
{
	check_displayid_datablock_revision(x);
	if (check_displayid_datablock_length(x))
		printf("    %s\n", extract_string(x + 3, x[2]));
}

// tag 0x12

static void parse_displayid_tiled_display_topology(const unsigned char *x)
{
	check_displayid_datablock_revision(x);

	if (!check_displayid_datablock_length(x, 22, 22))
		return;

	unsigned capabilities = x[3];
	unsigned num_v_tile = (x[4] & 0xf) | (x[6] & 0x30);
	unsigned num_h_tile = (x[4] >> 4) | ((x[6] >> 2) & 0x30);
	unsigned tile_v_location = (x[5] & 0xf) | ((x[6] & 0x3) << 4);
	unsigned tile_h_location = (x[5] >> 4) | (((x[6] >> 2) & 0x3) << 4);
	unsigned tile_width = x[7] | (x[8] << 8);
	unsigned tile_height = x[9] | (x[10] << 8);
	unsigned pix_mult = x[11];

	printf("    Capabilities: 0x%08x\n", capabilities);
	printf("    Num horizontal tiles: %u Num vertical tiles: %u\n",
	       num_h_tile + 1, num_v_tile + 1);
	printf("    Tile location: %u, %u\n", tile_h_location, tile_v_location);
	printf("    Tile resolution: %ux%u\n", tile_width + 1, tile_height + 1);
	if (capabilities & 0x40) {
		if (pix_mult) {
			printf("    Top bevel size: %u pixels\n",
			       pix_mult * x[12] / 10);
			printf("    Bottom bevel size: %u pixels\n",
			       pix_mult * x[13] / 10);
			printf("    Right bevel size: %u pixels\n",
			       pix_mult * x[14] / 10);
			printf("    Left bevel size: %u pixels\n",
			       pix_mult * x[15] / 10);
		} else {
			fail("No bevel information, but the pixel multiplier is non-zero\n");
		}
		printf("    Tile resolution: %ux%u\n", tile_width + 1, tile_height + 1);
	} else if (pix_mult) {
		fail("No bevel information, but the pixel multiplier is non-zero\n");
	}
}

// tag 0x26

static const char *bpc444[] = {"6", "8", "10", "12", "14", "16", NULL, NULL};
static const char *bpc4xx[] = {"8", "10", "12", "14", "16", NULL, NULL, NULL};
static const char *audiorates[] = {"32", "44.1", "48", NULL, NULL, NULL, NULL, NULL};

static const char *colorspace_eotf_combinations[] = {
	"sRGB",
	"BT.601",
	"BT.709/BT.1886",
	"Adobe RGB",
	"DCI-P3",
	"BT.2020",
	"BT.2020/SMPTE ST 2084"
};

static const char *colorspace_eotf_reserved[] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };

static const char *colorspaces[] = {
	"Undefined",
	"sRGB",
	"BT.601",
	"BT.709",
	"Adobe RGB",
	"DCI-P3",
	"BT.2020",
	"Custom"
};

static const char *eotfs[] = {
	"Undefined",
	"sRGB",
	"BT.601",
	"BT.1886",
	"Adobe RGB",
	"DCI-P3",
	"BT.2020",
	"Gamma function",
	"SMPTE ST 2084",
	"Hybrid Log",
	"Custom"
};

static void print_flags(const char *label, unsigned char flag_byte,
			const char **flags, bool reverse = false)
{
	if (!flag_byte)
		return;

	unsigned countflags = 0;

	printf("%s: ", label);
	for (unsigned i = 0; i < 8; i++) {
		if (flag_byte & (1 << (reverse ? 7 - i : i))) {
			if (countflags)
				printf(", ");
			if (flags[i])
				printf("%s", flags[i]);
			else
				printf("Undefined (%u)", i);
			countflags++;
		}
	}
	printf("\n");
}

static void parse_displayid_interface_features(const unsigned char *x)
{
	check_displayid_datablock_revision(x);

	if (!check_displayid_datablock_length(x, 9))
		return;

	unsigned len = x[2];
	if (len > 0) print_flags("    Supported bpc for RGB encoding", x[3], bpc444);
	if (len > 1) print_flags("    Supported bpc for YCbCr 4:4:4 encoding", x[4], bpc444);
	if (len > 2) print_flags("    Supported bpc for YCbCr 4:2:2 encoding", x[5], bpc4xx);
	if (len > 3) print_flags("    Supported bpc for YCbCr 4:2:0 encoding", x[6], bpc4xx);
	if (len > 4 && x[7])
		printf("    Minimum pixel rate at which YCbCr 4:2:0 encoding is supported: %.3f MHz\n",
		       74.25 * x[7]);
	if (len > 5) print_flags("    Supported audio capability and features (kHz)",
				 x[8], audiorates, true);
	if (len > 6) print_flags("    Supported color space and EOTF standard combination 1",
				 x[9], colorspace_eotf_combinations);
	if (len > 7) print_flags("    Supported color space and EOTF standard combination 2",x[10], colorspace_eotf_reserved);

	unsigned i = 0;

	if (len > 8 && x[11]) {
		printf("    Supported color space and EOTF additional combinations:");
		for (i = 0; i < x[11]; i++) {
			if (i > 6) {
				printf("\n    Number of additional color space and EOTF combinations (%d) is greater than allowed (7).", x[11]);
				break;
			} else if (i + 10 > len) {
				printf("\n    Number of additional color space and EOTF combinations (%d) is too many to fit in block (%d).", x[11], len - 9);
				break;
			}

			const char *colorspace = "Out of range";
			const char *eotf = "Out of range";
			unsigned colorspace_index = (x[12 + i] >> 4) & 0xf;
			unsigned eotf_index = x[12 + i] & 0xf;

			if (colorspace_index < sizeof(colorspaces) / sizeof(colorspaces[0]))
				colorspace = colorspaces[colorspace_index];
			if (eotf_index < sizeof(eotfs) / sizeof(eotfs[0]))
				eotf = eotfs[eotf_index];

			if (i > 0)
				printf(", ");
			if (!strcmp(colorspace, eotf))
				printf("%s", colorspace);
			else
				printf("%s/%s", colorspace, eotf);
		}
		printf("\n");
	}
	check_displayid_datablock_length(x, 9 + i, 9 + i, 9 + i);
}

// tag 0x29

static void parse_displayid_ContainerID(const unsigned char *x)
{
	check_displayid_datablock_revision(x);

	if (check_displayid_datablock_length(x, 16, 16)) {
		x += 3;
		printf("    %02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
		       x[0], x[1], x[2], x[3],
		       x[4], x[5],
		       x[6], x[7],
		       x[8], x[9],
		       x[10], x[11], x[12], x[13], x[14], x[15]);
	}
}

// DisplayID main

static std::string product_type(unsigned version, unsigned char x, bool heading)
{
	std::string headingstr;

	if (version < 0x20) {
		headingstr = "Display Product Type";
		if (heading) return headingstr;
		switch (x) {
		case 0: return "Extension Section";
		case 1: return "Test Structure; test equipment only";
		case 2: return "Display panel or other transducer, LCD or PDP module, etc.";
		case 3: return "Standalone display device";
		case 4: return "Television receiver";
		case 5: return "Repeater/translator";
		case 6: return "DIRECT DRIVE monitor";
		default: break;
		}
	} else {
		headingstr = "Display Product Primary Use Case";
		if (heading) return headingstr;
		switch (x) {
		case 0: return "Same primary use case as the base section";
		case 1: return "Test Structure; test equipment only";
		case 2: return "None of the listed primary use cases; generic display";
		case 3: return "Television (TV) display";
		case 4: return "Desktop productivity display";
		case 5: return "Desktop gaming display";
		case 6: return "Presentation display";
		case 7: return "Head-mounted Virtual Reality (VR) display";
		case 8: return "Head-mounted Augmented Reality (AR) display";
		default: break;
		}
	}
	fail("Unknown %s 0x%02x\n", headingstr.c_str(), x);
	return std::string("Unknown " + headingstr + " (") + utohex(x) + ")";
}

void edid_state::parse_displayid_block(const unsigned char *x)
{
	unsigned version = x[1];
	unsigned length = x[2];
	unsigned prod_type = x[3]; // future check: based on type, check for required data blocks
	unsigned ext_count = x[4];
	unsigned i;

	printf("%s Version %u.%u Length %u Extension Count %u\n",
	       block.c_str(), version >> 4, version & 0xf,
	       length, ext_count);

	if (ext_count > 0)
		warn("Non-0 DisplayID extension count %d\n", ext_count);

	printf("%s: %s\n", product_type(version, prod_type, true).c_str(),
	       product_type(version, prod_type, false).c_str());

	if (length > 121) {
		fail("DisplayID length %d is greater than 121\n", length);
		length = 121;
	}

	unsigned offset = 5;
	while (length > 0) {
		unsigned tag = x[offset];

		switch (tag) {
			// DisplayID 1.3:
		case 0x00: data_block = "Product Identification Data Block (" + utohex(tag) + ")"; break;
		case 0x01: data_block = "Display Parameters Data Block (" + utohex(tag) + ")"; break;
		case 0x02: data_block = "Color Characteristics Data Block"; break;
		case 0x03: data_block = "Video Timing Modes Type 1 - Detailed Timings Data Block"; break;
		case 0x04: data_block = "Video Timing Modes Type 2 - Detailed Timings Data Block"; break;
		case 0x05: data_block = "Video Timing Modes Type 3 - Short Timings Data Block"; break;
		case 0x06: data_block = "Video Timing Modes Type 4 - DMT Timings Data Block"; break;
		case 0x07: data_block = "Supported Timing Modes Type 1 - VESA DMT Timings Data Block"; break;
		case 0x08: data_block = "Supported Timing Modes Type 2 - CTA Timings Data Block"; break;
		case 0x09: data_block = "Video Timing Range Data Block"; break;
		case 0x0a: data_block = "Product Serial Number Data Block"; break;
		case 0x0b: data_block = "GP ASCII String Data Block"; break;
		case 0x0c: data_block = "Display Device Data Data Block"; break;
		case 0x0d: data_block = "Interface Power Sequencing Data Block"; break;
		case 0x0e: data_block = "Transfer Characteristics Data Block"; break;
		case 0x0f: data_block = "Display Interface Data Block"; break;
		case 0x10: data_block = "Stereo Display Interface Data Block (" + utohex(tag) + ")"; break;
		case 0x11: data_block = "Video Timing Modes Type 5 - Short Timings Data Block"; break;
		case 0x12: data_block = "Tiled Display Topology Data Block (" + utohex(tag) + ")"; break;
		case 0x13: data_block = "Video Timing Modes Type 6 - Detailed Timings Data Block"; break;
			   // 0x14 .. 0x7e RESERVED for Additional VESA-defined Data Blocks
			   // DisplayID 2.0
		case 0x20: data_block = "Product Identification Data Block (" + utohex(tag) + ")"; break;
		case 0x21: data_block = "Display Parameters Data Block (" + utohex(tag) + ")"; break;
		case 0x22: data_block = "Video Timing Modes Type 7 - Detailed Timings Data Block"; break;
		case 0x23: data_block = "Video Timing Modes Type 8 - Enumerated Timing Codes Data Block"; break;
		case 0x24: data_block = "Video Timing Modes Type 9 - Formula-based Timings Data Block"; break;
		case 0x25: data_block = "Dynamic Video Timing Range Limits Data Block"; break;
		case 0x26: data_block = "Display Interface Features Data Block"; break;
		case 0x27: data_block = "Stereo Display Interface Data Block (" + utohex(tag) + ")"; break;
		case 0x28: data_block = "Tiled Display Topology Data Block (" + utohex(tag) + ")"; break;
		case 0x29: data_block = "ContainerID Data Block"; break;
			   // 0x2a .. 0x7d RESERVED for Additional VESA-defined Data Blocks
		case 0x7e: // DisplayID 2.0
		case 0x7f: data_block = "Vendor-specific Data Block (" + utohex(tag) + ")"; break; // DisplayID 1.3
			   // 0x7f .. 0x80 RESERVED
		case 0x81: data_block = "CTA DisplayID Data Block (" + utohex(tag) + ")"; break;
			   // 0x82 .. 0xff RESERVED
		default:   data_block = "Unknown DisplayID Data Block (" + utohex(tag) + ")"; break;
		}

		if (length < 3) {
			// report a problem when the remaining bytes are not 0.
			if (tag || x[offset + 1]) {
				fail("Not enough bytes remain (%d) for a DisplayID data block or the DisplayID filler is non-0\n", length);
			}
			break;
		}

		unsigned len = x[offset + 2];

		if (length < len + 3) {
			fail("The length of this DisplayID data block (%d) exceeds the number of bytes remaining (%d)\n", len + 3, length);
			break;
		}

		if (!tag && !len) {
			// A Product Identification Data Block with no payload bytes is not valid - assume this is the end.
			if (!memchk(x + offset, length)) {
				fail("Non-0 filler bytes in the DisplayID block\n");
			}
			break;
		}

		printf("  %s\n", data_block.c_str());

		switch (tag) {
		case 0x01: parse_displayid_parameters(x + offset); break;
		case 0x03:
			   for (i = 0; i < len / 20; i++) {
				   parse_displayid_detailed_timing(&x[offset + 3 + (i * 20)]);
			   }
			   break;
		case 0x07:
			   for (i = 0; i < min(len, 10) * 8; i++)
				   if (x[offset + 3 + i / 8] & (1 << (i % 8))) {
					   print_timings("    ", find_dmt_id(i + 1), "DMT");
				   }
			   break;
		case 0x08:
			   for (i = 0; i < min(len, 8) * 8; i++)
				   if (x[offset + 3 + i / 8] & (1 << (i % 8))) {
					   char suffix[16];
					   sprintf(suffix, "VIC %3u", i + 1);
					   print_timings("    ", vic_to_mode(i + 1), suffix);
				   }
			   break;
		case 0x0b: parse_displayid_gp_string(x + offset); break;
		case 0x12: parse_displayid_tiled_display_topology(x + offset); break;
		case 0x26: parse_displayid_interface_features(x + offset); break;
		case 0x29: parse_displayid_ContainerID(x + offset); break;
		default: hex_block("    ", x + offset + 3, len); break;
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
	do_checksum("  ", x + 1, x[2] + 5);

	if (!memchk(x + 1 + x[2] + 5, 0x7f - (1 + x[2] + 5))) {
		data_block = "Padding";
		fail("DisplayID padding contains non-zero bytes\n");
	}
}
