// SPDX-License-Identifier: MIT
/*
 * Copyright 2006-2012 Red Hat, Inc.
 * Copyright 2018-2019 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 *
 * Author: Adam Jackson <ajax@nwnk.net>
 * Maintainer: Hans Verkuil <hverkuil-cisco@xs4all.nl>
 */

#ifndef __EDID_DECODE_H_
#define __EDID_DECODE_H_

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

#define EDID_PAGE_SIZE 128U

// Video Timings
struct timings {
	unsigned x, y;
	unsigned refresh;
	unsigned ratio_w, ratio_h;
	unsigned hor_freq_hz, pixclk_khz;
	unsigned rb, interlaced;
};

struct edid_state {
	edid_state()
	{
		min_hor_freq_hz = 0xffffff;
		min_vert_freq_hz = 0xffffffff;
	}

	// Base block state
	unsigned edid_minor;
	bool has_name_descriptor;
	bool has_display_range_descriptor;
	bool has_serial_number;
	bool has_serial_string;
	bool supports_continuous_freq;
	bool supports_gtf;
	bool supports_cvt;
	bool uses_gtf;
	bool uses_cvt;

	unsigned min_display_hor_freq_hz;
	unsigned max_display_hor_freq_hz;
	unsigned min_display_vert_freq_hz;
	unsigned max_display_vert_freq_hz;
	unsigned max_display_pixclk_khz;
	unsigned max_display_width_mm;
	unsigned max_display_height_mm;

	// CTA-861 block state
	bool has_640x480p60_est_timing;
	bool has_cta861_vic_1;
	unsigned supported_hdmi_vic_codes;
	unsigned supported_hdmi_vic_vsb_codes;

	// Global state
	unsigned num_blocks;
	std::string cur_block;
	unsigned cur_block_nr;

	unsigned min_hor_freq_hz;
	unsigned max_hor_freq_hz;
	unsigned min_vert_freq_hz;
	unsigned max_vert_freq_hz;
	unsigned max_pixclk_khz;

	unsigned warnings;
	unsigned failures;
};

void warn(const char *fmt, ...);
void fail(const char *fmt, ...);
void do_checksum(const char *prefix, const unsigned char *x, size_t len);
void hex_block(const char *prefix, const unsigned char *x, unsigned length, bool show_ascii = true);
std::string block_name(unsigned char block);
void print_timings(edid_state &state, const char *prefix, const struct timings *t, const char *suffix);

void detailed_timings(edid_state &state, const char *prefix, const unsigned char *x);
const struct timings *find_dmt_id(unsigned char dmt_id);
void parse_base_block(edid_state &state, const unsigned char *edid);

void parse_cta_block(edid_state &state, const unsigned char *x);

void parse_displayid_block(edid_state &state, const unsigned char *x);

#endif
