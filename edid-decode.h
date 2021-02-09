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
#include <set>
#include <string.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

#define EDID_PAGE_SIZE 128U
#define EDID_MAX_BLOCKS 256U

#define RB_FLAG (1U << 7)

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
	// 0: no reduced blanking
	// 1: reduced blanking version 1
	// 2: reduced blanking version 2
	// 3: reduced blanking version 3 with a horizontal blank of 80
	// 3 | RB_FLAG: reduced blanking version 3 with a horizontal blank of 160
	unsigned rb;
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

struct timings_ext {
	timings_ext()
	{
		memset(&t, 0, sizeof(t));
	}
	timings_ext(unsigned svr, const std::string &_type)
	{
		memset(&t, 0, sizeof(t));
		t.hact = svr;
		type = _type;
	}
	timings_ext(const timings &_t, const std::string &_type, const std::string &_flags)
	{
		t = _t;
		type = _type;
		flags = _flags;
	}

	bool is_valid() const { return t.hact; }
	bool has_svr() const { return t.hact && !t.vact; }
	unsigned svr() const { return t.hact; }
	timings t;
	std::string type;
	std::string flags;
};

typedef std::vector<timings_ext> vec_timings_ext;

struct edid_state {
	edid_state()
	{
		// Global state
		edid_size = num_blocks = block_nr = 0;
		max_hor_freq_hz = max_vert_freq_hz = max_pixclk_khz = 0;
		min_hor_freq_hz = 0xffffff;
		min_vert_freq_hz = 0xffffffff;
		dtd_max_vsize_mm = dtd_max_hsize_mm = 0;
		warnings = failures = 0;
		has_cta = has_dispid = false;
		hide_serial_numbers = false;

		// Base block state
		base.edid_minor = 0;
		base.has_name_descriptor = base.has_display_range_descriptor =
			base.has_serial_number = base.has_serial_string =
			base.supports_continuous_freq = base.supports_gtf =
			base.supports_cvt = base.seen_non_detailed_descriptor =
			base.has_640x480p60_est_timing = base.has_spwg =
			base.preferred_is_also_native = false;
		base.detailed_block_cnt = base.dtd_cnt = 0;

		base.min_display_hor_freq_hz = base.max_display_hor_freq_hz =
			base.min_display_vert_freq_hz = base.max_display_vert_freq_hz =
			base.max_display_pixclk_khz = base.max_display_width_mm =
			base.max_display_height_mm = 0;

		// CTA-861 block state
		cta.has_vic_1 = cta.first_svd_might_be_preferred = cta.has_sldb =
			cta.has_hdmi = cta.has_vcdb = cta.has_vfpdb = false;
		cta.last_block_was_hdmi_vsdb = cta.have_hf_vsdb = cta.have_hf_scdb = false;
		cta.first_block = cta.first_svd = true;
		cta.supported_hdmi_vic_codes = cta.supported_hdmi_vic_vsb_codes = 0;
		memset(cta.vics, 0, sizeof(cta.vics));
		memset(cta.preparsed_has_vic, 0, sizeof(cta.preparsed_has_vic));
		cta.preparsed_phys_addr = 0xffff;
		cta.preparsed_speaker_count = 0;
		cta.preparsed_sld = false;
		cta.preparsed_sld_has_coord = false;
		cta.preparsed_total_dtds = 0;
		cta.preparsed_total_vtdbs = 0;
		cta.preparsed_has_t8vtdb = false;

		// DisplayID block state
		dispid.version = 0;
		dispid.native_width = dispid.native_height = 0;
		dispid.preparsed_color_ids = dispid.preparsed_xfer_ids = 0;
		dispid.preparsed_displayid_blocks = 0;
		dispid.is_base_block = true;
		dispid.is_display = dispid.has_product_identification =
			dispid.has_display_parameters = dispid.has_type_1_7 =
			dispid.has_display_interface_features = false;

		// Block Map block state
		block_map.saw_block_1 = false;
		block_map.saw_block_128 = false;
	}

	// Global state
	unsigned edid_size;
	unsigned num_blocks;
	unsigned block_nr;
	std::string block;
	std::string data_block;
	bool has_cta;
	bool has_dispid;
	bool hide_serial_numbers;

	unsigned min_hor_freq_hz;
	unsigned max_hor_freq_hz;
	double min_vert_freq_hz;
	double max_vert_freq_hz;
	unsigned max_pixclk_khz;
	unsigned dtd_max_hsize_mm;
	unsigned dtd_max_vsize_mm;

	unsigned warnings;
	unsigned failures;

	// Base block state
	struct {
		unsigned edid_minor;
		bool has_name_descriptor;
		bool has_display_range_descriptor;
		bool has_serial_number;
		bool has_serial_string;
		bool supports_continuous_freq;
		bool supports_gtf;
		bool supports_cvt;
		bool has_spwg;
		unsigned detailed_block_cnt;
		unsigned dtd_cnt;
		bool seen_non_detailed_descriptor;
		bool has_640x480p60_est_timing;
		bool preferred_is_also_native;
		timings_ext preferred_timing;

		unsigned min_display_hor_freq_hz;
		unsigned max_display_hor_freq_hz;
		unsigned min_display_vert_freq_hz;
		unsigned max_display_vert_freq_hz;
		unsigned max_display_pixclk_khz;
		unsigned max_display_width_mm;
		unsigned max_display_height_mm;
	} base;

	// CTA-861 block state
	struct {
		unsigned preparsed_total_dtds;
		vec_timings_ext vec_dtds;
		unsigned preparsed_total_vtdbs;
		vec_timings_ext vec_vtdbs;
		vec_timings_ext preferred_timings;
		bool preparsed_has_t8vtdb;
		// Keep track of the found Tag/Extended Tag pairs.
		// The unsigned value is equal to: (tag << 8) | ext_tag
		std::set<unsigned> found_tags;
		timings_ext t8vtdb;
		vec_timings_ext native_timings;
		bool has_vic_1;
		bool first_svd_might_be_preferred;
		unsigned char byte3;
		bool has_hdmi;
		bool has_vcdb;
		bool has_vfpdb;
		unsigned preparsed_speaker_count;
		bool preparsed_sld_has_coord;
		bool preparsed_sld;
		bool has_sldb;
		unsigned short preparsed_phys_addr;
		bool last_block_was_hdmi_vsdb;
		bool have_hf_vsdb, have_hf_scdb;
		bool first_block;
		bool first_svd;
		unsigned supported_hdmi_vic_codes;
		unsigned supported_hdmi_vic_vsb_codes;
		unsigned short vics[256][2];
		bool preparsed_has_vic[2][256];
		std::vector<unsigned char> preparsed_svds[2];
	} cta;

	// DisplayID block state
	struct {
		unsigned char version;
		unsigned short preparsed_color_ids;
		unsigned short preparsed_xfer_ids;
		unsigned preparsed_displayid_blocks;
		bool is_base_block;
		bool is_display;
		bool has_product_identification;
		bool has_display_parameters;
		bool has_type_1_7;
		bool has_display_interface_features;
		vec_timings_ext preferred_timings;
		unsigned native_width, native_height;
		// Keep track of the found CTA-861 Tag/Extended Tag pairs.
		// The unsigned value is equal to: (tag << 8) | ext_tag
		std::set<unsigned> found_tags;
	} dispid;

	// Block Map block state
	struct {
		bool saw_block_1;
		bool saw_block_128;
	} block_map;

	std::string dtd_type(unsigned dtd);
	std::string dtd_type() { return dtd_type(base.dtd_cnt); }
	bool print_timings(const char *prefix, const struct timings *t,
			   const char *type, const char *flags = "",
			   bool detailed = false, bool do_checks = true);
	bool print_timings(const char *prefix, const struct timings_ext &t,
			   bool detailed = false, bool do_checks = true)
	{
		return print_timings(prefix, &t.t, t.type.c_str(), t.flags.c_str(),
				     detailed, do_checks);
	};
	bool match_timings(const timings &t1, const timings &t2);
	void edid_gtf_mode(unsigned refresh, struct timings &t);
	void edid_cvt_mode(unsigned refresh, struct timings &t);
	timings calc_cvt_mode(unsigned refresh, unsigned hact, unsigned vact, unsigned rb);
	void detailed_cvt_descriptor(const char *prefix, const unsigned char *x, bool first);
	void print_standard_timing(const char *prefix, unsigned char b1, unsigned char b2,
				   bool gtf_only = false, unsigned vrefresh_offset = 60);
	void detailed_display_range_limits(const unsigned char *x);
	void detailed_epi(const unsigned char *x);
	void detailed_timings(const char *prefix, const unsigned char *x,
			      bool base_or_cta = true);
	void preparse_detailed_block(const unsigned char *x);
	void detailed_block(const unsigned char *x);
	void parse_base_block(const unsigned char *x);
	void check_base_block();

	void print_vic_index(const char *prefix, unsigned idx, const char *suffix, bool ycbcr420 = false);
	void cta_vcdb(const unsigned char *x, unsigned length);
	void cta_svd(const unsigned char *x, unsigned n, bool for_ycbcr420);
	void cta_y420cmdb(const unsigned char *x, unsigned length);
	void cta_vfpdb(const unsigned char *x, unsigned length);
	void cta_rcdb(const unsigned char *x, unsigned length);
	void cta_sldb(const unsigned char *x, unsigned length);
	void cta_preparse_sldb(const unsigned char *x, unsigned length);
	void cta_hdmi_block(const unsigned char *x, unsigned length);
	void cta_displayid_type_7(const unsigned char *x, unsigned length);
	void cta_displayid_type_8(const unsigned char *x, unsigned length);
	void cta_displayid_type_10(const unsigned char *x, unsigned length);
	void cta_ext_block(const unsigned char *x, unsigned length, bool duplicate);
	void cta_block(const unsigned char *x, bool duplicate);
	void preparse_cta_block(const unsigned char *x);
	void parse_cta_block(const unsigned char *x);
	void cta_resolve_svr(vec_timings_ext::iterator iter);
	void cta_resolve_svrs();
	void check_cta_blocks();

	void parse_digital_interface(const unsigned char *x);
	void parse_display_device(const unsigned char *x);
	void parse_display_caps(const unsigned char *x);
	void parse_display_xfer(const unsigned char *x);
	void parse_di_ext_block(const unsigned char *x);

	void check_displayid_datablock_revision(unsigned char hdr,
						unsigned char valid_flags = 0,
						unsigned char rev = 0);
	void parse_displayid_product_id(const unsigned char *x);
	std::string product_type(unsigned char x, bool heading);
	void parse_displayid_interface_features(const unsigned char *x);
	void parse_displayid_parameters(const unsigned char *x);
	void parse_displayid_parameters_v2(const unsigned char *x, unsigned block_rev);
	void parse_displayid_display_intf(const unsigned char *x);
	void parse_displayid_color_characteristics(const unsigned char *x);
	void parse_displayid_transfer_characteristics(const unsigned char *x);
	void parse_displayid_stereo_display_intf(const unsigned char *x);
	void parse_displayid_type_1_7_timing(const unsigned char *x,
					     bool type7, unsigned block_rev, bool is_cta = false);
	void parse_displayid_type_2_timing(const unsigned char *x);
	void parse_displayid_type_3_timing(const unsigned char *x);
	void parse_displayid_type_4_8_timing(unsigned char type, unsigned short id, bool is_cta = false);
	void parse_displayid_video_timing_range_limits(const unsigned char *x);
	void parse_displayid_string(const unsigned char *x);
	void parse_displayid_display_device(const unsigned char *x);
	void parse_displayid_intf_power_sequencing(const unsigned char *x);
	void parse_displayid_type_5_timing(const unsigned char *x);
	void parse_displayid_tiled_display_topology(const unsigned char *x, bool is_v2);
	void parse_displayid_type_6_timing(const unsigned char *x);
	void parse_displayid_type_9_timing(const unsigned char *x);
	void parse_displayid_dynamic_video_timings_range_limits(const unsigned char *x);
	void parse_displayid_ContainerID(const unsigned char *x);
	void parse_displayid_type_10_timing(const unsigned char *x, bool is_cta = false);
	void preparse_displayid_block(const unsigned char *x);
	void parse_displayid_block(const unsigned char *x);
	void parse_displayid_vesa(const unsigned char *x);
	void parse_displayid_cta_data_block(const unsigned char *x);
	void check_displayid_blocks();

	void parse_vtb_ext_block(const unsigned char *x);

	void parse_string_table(const unsigned char *x);
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

#ifdef _WIN32

#define warn(fmt, ...) msg(true, fmt, __VA_ARGS__)
#define warn_once(fmt, ...)				\
	do {						\
		static bool shown_warn;			\
		if (!shown_warn) {			\
			shown_warn = true;		\
			msg(true, fmt, __VA_ARGS__);	\
		}					\
	} while (0)
#define fail(fmt, ...) msg(false, fmt, __VA_ARGS__)

#else

#define warn(fmt, args...) msg(true, fmt, ##args)
#define warn_once(fmt, args...)				\
	do {						\
		static bool shown_warn;			\
		if (!shown_warn) {			\
			shown_warn = true;		\
			msg(true, fmt, ##args);		\
		}					\
	} while (0)
#define fail(fmt, args...) msg(false, fmt, ##args)

#endif

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
