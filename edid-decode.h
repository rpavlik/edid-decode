// SPDX-License-Identifier: MIT
/*
 * Copyright 2006-2012 Red Hat, Inc.
 * Copyright 2018-2020 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 *
 * Author: Adam Jackson <ajax@nwnk.net>
 * Maintainer: Hans Verkuil <hverkuil-cisco@xs4all.nl>
 */

#ifndef __EDID_DECODE_H_
#define __EDID_DECODE_H_

#include <string>
#include <vector>
#include <string.h>

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
	// The horizontal frontporch may be negative in GTF calculations,
	// so use int instead of unsigned for hfp. Example: 292x176@76.
	int hfp;
	unsigned hsync;
	// The backporch may be negative in buggy detailed timings.
	// So use int instead of unsigned for hbp and vbp.
	int hbp;
	bool pos_pol_hsync;
	// For interlaced formats the vertical front porch of the Even Field
	// is actually a half-line longer.
	unsigned vfp, vsync;
	// For interlaced formats the vertical back porch of the Odd Field
	// is actually a half-line longer.
	int vbp;
	bool pos_pol_vsync;
	unsigned hborder, vborder;
	bool even_vtotal; // special for VIC 39
	bool no_pol_vsync; // digital composite signals have no vsync polarity
	unsigned hsize_mm, vsize_mm;
	bool ycbcr420; // YCbCr 4:2:0 encoding
};

struct edid_state {
	edid_state()
	{
		// Global state
		edid_size = num_blocks = block_nr = 0;
		max_hor_freq_hz = max_vert_freq_hz = max_pixclk_khz = 0;
		min_hor_freq_hz = 0xffffff;
		min_vert_freq_hz = 0xffffffff;
		warnings = failures = 0;
		memset(&preferred_timings, 0, sizeof(preferred_timings));
		preparse_total_dtds = 0;

		// Base block state
		edid_minor = 0;
		has_name_descriptor = has_display_range_descriptor =
			has_serial_number = has_serial_string =
			supports_continuous_freq = supports_gtf =
			supports_cvt = uses_gtf = uses_cvt = has_spwg =
			seen_non_detailed_descriptor = false;
		detailed_block_cnt = dtd_cnt = 0;

		min_display_hor_freq_hz = max_display_hor_freq_hz =
			min_display_vert_freq_hz = max_display_vert_freq_hz =
			max_display_pixclk_khz = max_display_width_mm =
			max_display_height_mm = 0;

		// CTA-861 block state
		has_640x480p60_est_timing = has_cta861_vic_1 =
			first_svd_might_be_preferred = has_hdmi = false;
		last_block_was_hdmi_vsdb = have_hf_vsdb = have_hf_scdb = 0;
		first_block = 1;
		supported_hdmi_vic_codes = supported_hdmi_vic_vsb_codes = 0;
		memset(vics, 0, sizeof(vics));
		memset(preparsed_has_vic, 0, sizeof(preparsed_has_vic));

		// DisplayID block state
		preparse_color_ids = preparse_xfer_ids = 0;
		preparse_displayid_blocks = 0;
		displayid_base_block = true;

		// Block Map block state
		saw_block_1 = false;
	}

	// Global state
	unsigned edid_size;
	unsigned num_blocks;
	unsigned block_nr;
	std::string block;
	std::string data_block;
	timings preferred_timings;
	std::string preferred_type;
	std::string preferred_flags;
	unsigned preparse_total_dtds;

	unsigned min_hor_freq_hz;
	unsigned max_hor_freq_hz;
	double min_vert_freq_hz;
	double max_vert_freq_hz;
	unsigned max_pixclk_khz;

	unsigned warnings;
	unsigned failures;

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
	unsigned detailed_block_cnt;
	unsigned dtd_cnt;
	bool seen_non_detailed_descriptor;

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
	bool first_svd_might_be_preferred;
	bool has_hdmi;
	int last_block_was_hdmi_vsdb;
	int have_hf_vsdb, have_hf_scdb;
	int first_block;
	unsigned supported_hdmi_vic_codes;
	unsigned supported_hdmi_vic_vsb_codes;
	unsigned short vics[256][2];
	bool preparsed_has_vic[2][256];
	std::vector<unsigned char> preparsed_svds[2];

	// DisplayID block state
	unsigned short preparse_color_ids;
	unsigned short preparse_xfer_ids;
	unsigned preparse_displayid_blocks;
	bool displayid_base_block;

	// Block Map block state
	bool saw_block_1;

	std::string dtd_type();
	bool print_timings(const char *prefix, const struct timings *t,
			   const char *type, const char *flags = "",
			   bool detailed = false);
	bool match_timings(const timings &t1, const timings &t2);
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

	void print_vic_index(const char *prefix, unsigned idx, const char *suffix, bool ycbcr420 = false);
	void cta_svd(const unsigned char *x, unsigned n, bool for_ycbcr420);
	void cta_y420cmdb(const unsigned char *x, unsigned length);
	void cta_vfpdb(const unsigned char *x, unsigned length);
	void cta_hdmi_block(const unsigned char *x, unsigned length);
	void cta_ext_block(const unsigned char *x, unsigned length);
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
	void parse_displayid_cta_data_block(const unsigned char *x);

	void preparse_vtb_ext_block(const unsigned char *x);
	void parse_vtb_ext_block(const unsigned char *x);

	void parse_ls_ext_block(const unsigned char *x);

	void parse_block_map(const unsigned char *x);

	void preparse_extension(const unsigned char *x);
	void parse_extension(const unsigned char *x);
	int parse_edid();
};

static inline void add_str(std::string &s, const std::string &add)
{
	if (s.empty())
		s = add;
	else if (!add.empty())
		s = s + ", " + add;
}

void msg(bool is_warn, const char *fmt, ...);

#define warn(fmt, args...) msg(true, fmt, ##args)
#define fail(fmt, args...) msg(false, fmt, ##args)

void do_checksum(const char *prefix, const unsigned char *x, size_t len);
std::string utohex(unsigned char x);
std::string ouitohex(unsigned oui);
bool memchk(const unsigned char *x, unsigned len, unsigned char v = 0);
void hex_block(const char *prefix, const unsigned char *x, unsigned length,
	       bool show_ascii = true, unsigned step = 16);
std::string block_name(unsigned char block);
void calc_ratio(struct timings *t);
const char *oui_name(unsigned oui, bool reverse = false);

const struct timings *find_dmt_id(unsigned char dmt_id);
const struct timings *find_vic_id(unsigned char vic);
const struct timings *find_hdmi_vic_id(unsigned char hdmi_vic);
unsigned char hdmi_vic_to_vic(unsigned char hdmi_vic);
char *extract_string(const unsigned char *x, unsigned len);

#endif
