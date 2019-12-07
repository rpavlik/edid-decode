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
// for each field is (vfp + vsync + vbp + 0.5), except for
// the VIC 39 timings that doesn't have the 0.5 constant.
struct timings {
	// Active horizontal and vertical frame height, including any
	// borders, if present.
	// Note: for interlaced formats the active field height is vact / 2
	unsigned hact, vact;
	unsigned hratio, vratio;
	unsigned pixclk_khz;
	unsigned rb; // 1 if CVT with reduced blanking, 2 if CVT with reduced blanking v2
	bool interlaced;
	// The backporch may be negative in buggy detailed timings.
	// So use int instead of unsigned for hbp and vbp.
	unsigned hfp, hsync;
	int hbp;
	bool pos_pol_hsync;
	unsigned vfp, vsync;
	int vbp;
	bool pos_pol_vsync;
	unsigned hborder, vborder;
	bool even_vtotal; // special for VIC 39
	bool no_pol_vsync; // digital composite signals have no vsync polarity
	unsigned hsize_mm, vsize_mm;
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
	bool preparsed_has_vic[2][256];
	std::vector<unsigned char> preparsed_svds[2];

	// DisplayID block state
	unsigned short preparse_color_ids;
	unsigned short preparse_xfer_ids;

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
	bool print_detailed_timings(const char *prefix, const struct timings &t, const char *flags);
	void edid_gtf_mode(unsigned refresh, struct timings &t);
	void edid_cvt_mode(unsigned refresh, struct timings &t);
	void detailed_cvt_descriptor(const char *prefix, const unsigned char *x, bool first);
	void print_standard_timing(const char *prefix, unsigned char b1, unsigned char b2,
				   bool gtf_only = false, unsigned vrefresh_offset = 60);
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

	void parse_digital_interface(const unsigned char *x);
	void parse_display_device(const unsigned char *x);
	void parse_display_caps(const unsigned char *x);
	void parse_display_xfer(const unsigned char *x);
	void parse_di_ext_block(const unsigned char *x);

	void parse_displayid_color_characteristics(const unsigned char *x);
	void parse_displayid_transfer_characteristics(const unsigned char *x);
	void parse_displayid_stereo_display_intf(const unsigned char *x);
	void parse_displayid_type_1_7_timing(const unsigned char *x, bool type7);
	void parse_displayid_type_2_timing(const unsigned char *x);
	void parse_displayid_type_3_timing(const unsigned char *x);
	void parse_displayid_type_4_8_timing(unsigned char type, unsigned short id);
	void parse_displayid_type_5_timing(const unsigned char *x);
	void parse_displayid_type_6_timing(const unsigned char *x);
	void parse_displayid_type_9_timing(const unsigned char *x);
	void preparse_displayid_block(const unsigned char *x);
	void parse_displayid_block(const unsigned char *x);

	void parse_vtb_ext_block(const unsigned char *x);

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
void calc_ratio(struct timings *t);

const struct timings *find_dmt_id(unsigned char dmt_id);
const struct timings *find_vic_id(unsigned char vic);
const struct timings *find_hdmi_vic_id(unsigned char hdmi_vic);
char *extract_string(const unsigned char *x, unsigned len);

#endif
