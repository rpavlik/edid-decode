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

#include <string>
#include <vector>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

#define EDID_PAGE_SIZE 128U
#define EDID_MAX_BLOCKS 256U

// Video Timings
// If interlaced is true, then the vertical blanking
// for each field is (vfp + vsync + vbp + 0.5)
struct timings {
	unsigned w, h;
	unsigned ratio_w, ratio_h;
	unsigned pixclk_khz;
	bool rb, interlaced;
	// The backporch may be negative in buggy detailed timings.
	// So use int instead of unsigned for hbp and vbp.
	unsigned hfp, hsync;
	int hbp;
	bool pos_pol_hsync;
	unsigned vfp, vsync;
	int vbp;
	bool pos_pol_vsync;
	unsigned hborder, vborder;
	unsigned hor_mm, vert_mm;
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
	bool has_spwg;
	int timing_descr_cnt;

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
	std::vector<unsigned char> svds;

	// Global state
	unsigned edid_size;
	unsigned num_blocks;
	unsigned block_nr;
	std::string block;
	std::string data_block;

	unsigned min_hor_freq_hz;
	unsigned max_hor_freq_hz;
	unsigned min_vert_freq_hz;
	unsigned max_vert_freq_hz;
	unsigned max_pixclk_khz;

	unsigned warnings;
	unsigned failures;

	void print_timings(const char *prefix, const struct timings *t,
			   const char *suffix);

	void edid_gtf_mode(unsigned refresh, struct timings &t);
	void edid_cvt_mode(unsigned refresh, struct timings &t);
	void detailed_cvt_descriptor(const unsigned char *x, bool first);
	void print_standard_timing(unsigned char b1, unsigned char b2);
	void detailed_display_range_limits(const unsigned char *x);
	void detailed_epi(const unsigned char *x);
	void detailed_timings(const char *prefix, const unsigned char *x);
	void detailed_block(const unsigned char *x);
	void parse_base_block(const unsigned char *x);

	void print_vic_index(const char *prefix, unsigned idx, const char *suffix);
	void cta_svd(const unsigned char *x, unsigned n, int for_ycbcr420);
	void cta_y420cmdb(const unsigned char *x, unsigned length);
	void cta_vfpdb(const unsigned char *x, unsigned length);
	void cta_hdmi_block(const unsigned char *x, unsigned length);
	void cta_block(const unsigned char *x);
	void preparse_cta_block(const unsigned char *x);
	void parse_cta_block(const unsigned char *x);

	void parse_displayid_block(const unsigned char *x);

	void parse_digital_interface(const unsigned char *x);
	void parse_display_device(const unsigned char *x);
	void parse_display_caps(const unsigned char *x);
	void parse_display_xfer(const unsigned char *x);
	void parse_di_ext_block(const unsigned char *x);

	void parse_ls_ext_block(const unsigned char *x);

	void parse_block_map(const unsigned char *x);

	void preparse_extension(const unsigned char *x);
	void parse_extension(const unsigned char *x);
	int parse_edid();
};

void msg(bool is_warn, const char *fmt, ...);

#define warn(fmt, args...) msg(true, fmt, ##args)
#define fail(fmt, args...) msg(false, fmt, ##args)

void do_checksum(const char *prefix, const unsigned char *x, size_t len);
std::string utohex(unsigned char x);
bool memchk(const unsigned char *x, unsigned len, unsigned char v = 0);
void hex_block(const char *prefix, const unsigned char *x, unsigned length,
	       bool show_ascii = true, unsigned step = 16);
std::string block_name(unsigned char block);
void print_timings(edid_state &state, const char *prefix, const struct timings *t, const char *suffix);

const struct timings *find_dmt_id(unsigned char dmt_id);

#endif
