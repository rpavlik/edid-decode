/*
 * Copyright 2006-2012 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
/* Author: Adam Jackson <ajax@nwnk.net> */
/* Maintainer: Hans Verkuil <hverkuil-cisco@xs4all.nl> */

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

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

enum {
	EDID_PAGE_SIZE = 128u
};

static unsigned edid_minor = 0;
static int claims_one_point_oh = 0;
static int claims_one_point_two = 0;
static int claims_one_point_three = 0;
static int claims_one_point_four = 0;
static int nonconformant_digital_display = 0;
static int nonconformant_extension = 0;
static int did_detailed_timing = 0;
static int has_name_descriptor = 0;
static int has_range_descriptor = 0;
static int has_preferred_timing = 0;
static int has_valid_checksum = 1;
static int has_valid_cta_checksum = 1;
static int has_valid_displayid_checksum = 1;
static int has_serial_number = 0;
static int has_serial_string = 0;
static int has_valid_name_descriptor = 0;
static int has_valid_detailed_blocks = 0;
static int has_valid_descriptor_ordering = 1;
static int has_cta861 = 0;
static int has_640x480p60_est_timing = 0;
static int has_cta861_vic_1 = 0;
static int seen_non_detailed_descriptor = 0;

static int nonconformant_hf_vsdb_position = 0;
static int duplicate_scdb = 0;
static int nonconformant_cta861_640x480 = 0;
static int nonconformant_hdmi_vsdb_tmds_rate = 0;
static int nonconformant_hf_vsdb_tmds_rate = 0;
static int nonconformant_hf_eeodb = 0;

static unsigned min_hor_freq_hz = 0xfffffff;
static unsigned max_hor_freq_hz = 0;
static unsigned min_vert_freq_hz = 0xfffffff;
static unsigned max_vert_freq_hz = 0;
static unsigned max_pixclk_khz = 0;
static unsigned mon_min_hor_freq_hz = 0;
static unsigned mon_max_hor_freq_hz = 0;
static unsigned mon_min_vert_freq_hz = 0;
static unsigned mon_max_vert_freq_hz = 0;
static unsigned mon_max_pixclk_khz = 0;
static unsigned max_display_width_mm = 0;
static unsigned max_display_height_mm = 0;

static unsigned supported_hdmi_vic_codes = 0;
static unsigned supported_hdmi_vic_vsb_codes = 0;

static int conformant = 1;
static unsigned warnings;
static unsigned fails;

enum output_format {
	OUT_FMT_DEFAULT,
	OUT_FMT_HEX,
	OUT_FMT_RAW,
	OUT_FMT_CARRAY
};

// Short Timings (for DMT and Established Timings I && II)
struct short_timings {
	unsigned x, y;
	unsigned refresh;
	unsigned ratio_w, ratio_h;
	unsigned hor_freq_hz, pixclk_khz;
	unsigned rb, interlaced;
};

/*
 * Options
 * Please keep in alphabetical order of the short option.
 * That makes it easier to see which options are still free.
 */
enum Option {
	OptCheck = 'c',
	OptExtract = 'e',
	OptHelp = 'h',
	OptOutputFormat = 'o',
	OptLast = 256
};

static char options[OptLast];

static struct option long_options[] = {
	{ "help", no_argument, 0, OptHelp },
	{ "output-format", required_argument, 0, OptOutputFormat },
	{ "extract", no_argument, 0, OptExtract },
	{ "check", no_argument, 0, OptCheck },
	{ 0, 0, 0, 0 }
};

static void usage(void)
{
	printf("Usage: edid-decode <options> [in [out]]\n"
	       "  [in]                  EDID file to parse. Read from standard input if none given\n"
	       "                        or if the input filename is '-'.\n"
	       "  [out]                 Output the read EDID to this file. Write to standard output\n"
	       "                        if the output filename is '-'.\n"
	       "\nOptions:\n"
	       "  -o, --output-format <fmt>\n"
	       "                        if [out] is specified, then write the EDID in this format\n"
	       "                        <fmt> is one of:\n"
	       "                        hex:    hex numbers in ascii text (default for stdout)\n"
	       "                        raw:    binary data (default unless writing to stdout)\n"
	       "                        carray: c-program struct\n"
	       "  -c, --check           check if the EDID conforms to the standards\n"
	       "  -e, --extract         extract the contents of the first block in hex values\n"
	       "  -h, --help            display this help message\n");
}

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

static const char *cur_block;
static char *s_warn;
static unsigned s_warn_len = 1;
static char *s_fail;
static unsigned s_fail_len = 1;

static void warn(const char *fmt, ...)
{
	unsigned length;
	char buf[256];
	va_list ap;

	va_start(ap, fmt);
	vsprintf(buf, fmt, ap);
	va_end(ap);
	warnings++;
	length = strlen(buf);
	s_warn = realloc(s_warn, s_warn_len + length + strlen(cur_block) + 2);
	strcpy(s_warn + s_warn_len - 1, cur_block);
	s_warn_len += strlen(cur_block);
	strcpy(s_warn + s_warn_len - 1, ": ");
	s_warn_len += 2;
	strcpy(s_warn + s_warn_len - 1, buf);
	s_warn_len += length;
}

static void fail(const char *fmt, ...)
{
	unsigned length;
	char buf[256];
	va_list ap;

	va_start(ap, fmt);
	vsprintf(buf, fmt, ap);
	va_end(ap);
	fails++;
	conformant = 0;
	length = strlen(buf);
	s_fail = realloc(s_fail, s_fail_len + length + strlen(cur_block) + 2);
	strcpy(s_fail + s_fail_len - 1, cur_block);
	s_fail_len += strlen(cur_block);
	strcpy(s_fail + s_fail_len - 1, ": ");
	s_fail_len += 2;
	strcpy(s_fail + s_fail_len - 1, buf);
	s_fail_len += length;
}

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

static char *manufacturer_name(const unsigned char *x)
{
	static char name[4];

	name[0] = ((x[0] & 0x7c) >> 2) + '@';
	name[1] = ((x[0] & 0x03) << 3) + ((x[1] & 0xe0) >> 5) + '@';
	name[2] = (x[1] & 0x1f) + '@';
	name[3] = 0;

	if (!isupper(name[0]) || !isupper(name[1]) || !isupper(name[2]))
		fail("manufacturer name field contains garbage\n");

	return name;
}

static const struct {
	unsigned dmt_id;
	unsigned std_id;
	unsigned cvt_id;
	struct short_timings t;
} dmt_timings[] = {
	{ 0x01, 0x0000, 0x000000, { 640, 350, 85, 64, 35, 37900, 31500 } },

	{ 0x02, 0x3119, 0x000000, { 640, 400, 85, 16, 10, 37900, 31500 } },

	{ 0x03, 0x0000, 0x000000, { 720, 400, 85, 9, 5, 37900, 35500 } },

	{ 0x04, 0x3140, 0x000000, { 640, 480, 60, 4, 3, 31469, 25175 } },
	{ 0x05, 0x314c, 0x000000, { 640, 480, 72, 4, 3, 37900, 31500 } },
	{ 0x06, 0x314f, 0x000000, { 640, 480, 75, 4, 3, 37500, 31500 } },
	{ 0x07, 0x3159, 0x000000, { 640, 480, 85, 4, 3, 43300, 36000 } },

	{ 0x08, 0x0000, 0x000000, { 800, 600, 56, 4, 3, 35200, 36000 } },
	{ 0x09, 0x4540, 0x000000, { 800, 600, 60, 4, 3, 37900, 40000 } },
	{ 0x0a, 0x454c, 0x000000, { 800, 600, 72, 4, 3, 48100, 50000 } },
	{ 0x0b, 0x454f, 0x000000, { 800, 600, 75, 4, 3, 46900, 49500 } },
	{ 0x0c, 0x4559, 0x000000, { 800, 600, 85, 4, 3, 53700, 56250 } },
	{ 0x0d, 0x0000, 0x000000, { 800, 600, 120, 4, 3, 76302, 73250, 1 } },

	{ 0x0e, 0x0000, 0x000000, { 848, 480, 60, 16, 9, 31020, 33750 } },

	{ 0x0f, 0x0000, 0x000000, { 1024, 768, 43, 4, 3, 35522, 44900, 0, 1 } },
	{ 0x10, 0x6140, 0x000000, { 1024, 768, 60, 4, 3, 48400, 65000 } },
	{ 0x11, 0x614c, 0x000000, { 1024, 768, 70, 4, 3, 56500, 75000 } },
	{ 0x12, 0x614f, 0x000000, { 1024, 768, 75, 4, 3, 60000, 78750 } },
	{ 0x13, 0x6159, 0x000000, { 1024, 768, 85, 4, 3, 68700, 94500 } },
	{ 0x14, 0x0000, 0x000000, { 1024, 768, 120, 4, 3, 97551, 115500, 1 } },

	{ 0x15, 0x714f, 0x000000, { 1152, 864, 75, 4, 3, 67500, 108000 } },

	{ 0x55, 0x81c0, 0x000000, { 1280, 720, 60, 16, 9, 45000, 74250 } },

	{ 0x16, 0x0000, 0x7f1c21, { 1280, 768, 60, 5, 3, 47400, 68250, 1 } },
	{ 0x17, 0x0000, 0x7f1c28, { 1280, 768, 60, 5, 3, 47800, 79500 } },
	{ 0x18, 0x0000, 0x7f1c44, { 1280, 768, 75, 5, 3, 60300, 102250 } },
	{ 0x19, 0x0000, 0x7f1c62, { 1280, 768, 85, 5, 3, 68600, 117500 } },
	{ 0x1a, 0x0000, 0x000000, { 1280, 768, 120, 5, 3, 97396, 140250, 1 } },

	{ 0x1b, 0x0000, 0x8f1821, { 1280, 800, 60, 16, 10, 49306, 710000, 1 } },
	{ 0x1c, 0x8100, 0x8f1828, { 1280, 800, 60, 16, 10, 49702, 83500 } },
	{ 0x1d, 0x810f, 0x8f1844, { 1280, 800, 75, 16, 10, 62795, 106500 } },
	{ 0x1e, 0x8119, 0x8f1862, { 1280, 800, 85, 16, 10, 71554, 122500 } },
	{ 0x1f, 0x0000, 0x000000, { 1280, 800, 120, 16, 10, 101563, 146250, 1 } },

	{ 0x20, 0x8140, 0x000000, { 1280, 960, 60, 4, 3, 60000, 108000 } },
	{ 0x21, 0x8159, 0x000000, { 1280, 960, 85, 4, 3, 85900, 148500 } },
	{ 0x22, 0x0000, 0x000000, { 1280, 960, 120, 4, 3, 121875, 175500, 1 } },

	{ 0x23, 0x8180, 0x000000, { 1280, 1024, 60, 5, 4, 64000, 108000 } },
	{ 0x24, 0x818f, 0x000000, { 1280, 1024, 75, 5, 4, 80000, 135000 } },
	{ 0x25, 0x8199, 0x000000, { 1280, 1024, 85, 5, 4, 91100, 157500 } },
	{ 0x26, 0x0000, 0x000000, { 1280, 1024, 120, 5, 4, 130035, 187250, 1 } },

	{ 0x27, 0x0000, 0x000000, { 1360, 768, 60, 85, 48, 47700, 85500 } },
	{ 0x28, 0x0000, 0x000000, { 1360, 768, 120, 85, 48, 97533, 148250, 1 } },

	{ 0x51, 0x0000, 0x000000, { 1366, 768, 60, 85, 48, 47700, 85500 } },
	{ 0x56, 0x0000, 0x000000, { 1366, 768, 60, 85, 48, 48000, 72000, 1 } },

	{ 0x29, 0x0000, 0x0c2021, { 1400, 1050, 60, 4, 3, 64700, 101000, 1 } },
	{ 0x2a, 0x9040, 0x0c2028, { 1400, 1050, 60, 4, 3, 65300, 121750 } },
	{ 0x2b, 0x904f, 0x0c2044, { 1400, 1050, 75, 4, 3, 82300, 156000 } },
	{ 0x2c, 0x9059, 0x0c2062, { 1400, 1050, 85, 4, 3, 93900, 179500 } },
	{ 0x2d, 0x0000, 0x000000, { 1400, 1050, 120, 4, 3, 133333, 208000, 1 } },

	{ 0x2e, 0x0000, 0xc11821, { 1440, 900, 60, 16, 10, 55500, 88750, 1 } },
	{ 0x2f, 0x9500, 0xc11828, { 1440, 900, 60, 16, 10, 65300, 121750 } },
	{ 0x30, 0x950f, 0xc11844, { 1440, 900, 75, 16, 10, 82300, 156000 } },
	{ 0x31, 0x9519, 0xc11868, { 1440, 900, 85, 16, 10, 93900, 179500 } },
	{ 0x32, 0x0000, 0x000000, { 1440, 900, 120, 16, 10, 114219, 182750, 1 } },

	{ 0x53, 0xa9c0, 0x000000, { 1600, 900, 60, 16, 9, 60000, 108000, 1 } },

	{ 0x33, 0xa940, 0x000000, { 1600, 1200, 60, 4, 3, 75000, 162000 } },
	{ 0x34, 0xa945, 0x000000, { 1600, 1200, 65, 4, 3, 81300, 175500 } },
	{ 0x35, 0xa94a, 0x000000, { 1600, 1200, 70, 4, 3, 87500, 189000 } },
	{ 0x36, 0xa94f, 0x000000, { 1600, 1200, 75, 4, 3, 93800, 202500 } },
	{ 0x37, 0xa959, 0x000000, { 1600, 1200, 85, 4, 3, 106300, 229500 } },
	{ 0x38, 0x0000, 0x000000, { 1600, 1200, 120, 4, 3, 152415, 268250, 1 } },

	{ 0x39, 0x0000, 0x0c2821, { 1680, 1050, 60, 16, 10, 64700, 119000, 1 } },
	{ 0x3a, 0xb300, 0x0c2828, { 1680, 1050, 60, 16, 10, 65300, 146250 } },
	{ 0x3b, 0xb30f, 0x0c2844, { 1680, 1050, 75, 16, 10, 82300, 187000 } },
	{ 0x3c, 0xb319, 0x0c2868, { 1680, 1050, 85, 16, 10, 93900, 214750 } },
	{ 0x3d, 0x0000, 0x000000, { 1680, 1050, 120, 16, 10, 133424, 245500, 1 } },

	{ 0x3e, 0xc140, 0x000000, { 1792, 1344, 60, 4, 3, 83600, 204750 } },
	{ 0x3f, 0xc14f, 0x000000, { 1792, 1344, 75, 4, 3, 106300, 261000 } },
	{ 0x40, 0x0000, 0x000000, { 1792, 1344, 120, 4, 3, 170722, 333250, 1 } },

	{ 0x41, 0xc940, 0x000000, { 1856, 1392, 60, 4, 3, 86300, 218250 } },
	{ 0x42, 0xc94f, 0x000000, { 1856, 1392, 75, 4, 3, 112500, 288000 } },
	{ 0x43, 0x0000, 0x000000, { 1856, 1392, 120, 4, 3, 176835, 356500, 1 } },

	{ 0x52, 0xd1c0, 0x000000, { 1920, 1080, 60, 16, 9, 675000, 148500 } },

	{ 0x44, 0x0000, 0x572821, { 1920, 1200, 60, 16, 10, 74000, 154000, 1 } },
	{ 0x45, 0xd100, 0x572828, { 1920, 1200, 60, 16, 10, 74600, 193250 } },
	{ 0x46, 0xd10f, 0x572844, { 1920, 1200, 75, 16, 10, 94000, 245250 } },
	{ 0x47, 0xd119, 0x572862, { 1920, 1200, 85, 16, 10, 107200, 281250 } },
	{ 0x48, 0x0000, 0x000000, { 1920, 1200, 120, 16, 10, 152404, 317000, 1 } },

	{ 0x49, 0xd140, 0x000000, { 1920, 1440, 60, 4, 3, 90000, 234000 } },
	{ 0x4a, 0xd14f, 0x000000, { 1920, 1440, 75, 4, 3, 112500, 297000 } },
	{ 0x4b, 0x0000, 0x000000, { 1920, 1440, 120, 4, 3, 182933, 380500, 1 } },

	{ 0x54, 0xe1c0, 0x000000, { 2048, 1152, 60, 16, 9, 72000, 162000, 1 } },

	{ 0x4c, 0x0000, 0x1f3821, { 2560, 1600, 60, 16, 10, 98713, 268500, 1 } },
	{ 0x4d, 0x0000, 0x1f3828, { 2560, 1600, 60, 16, 10, 99458, 348500 } },
	{ 0x4e, 0x0000, 0x1f3844, { 2560, 1600, 75, 16, 10, 125354, 443250 } },
	{ 0x4f, 0x0000, 0x1f3862, { 2560, 1600, 85, 16, 10, 142887, 505250 } },
	{ 0x50, 0x0000, 0x000000, { 2560, 1600, 120, 16, 10, 203217, 552750, 1 } },

	{ 0x57, 0x0000, 0x000000, { 4096, 2160, 60, 256, 135, 133320, 556744, 1 } },
	{ 0x58, 0x0000, 0x000000, { 4096, 2160, 59, 256, 135, 133187, 556188, 1 } },
};

static const struct short_timings *find_dmt_id(unsigned char dmt_id)
{
	unsigned i;

	for (i = 0; i < ARRAY_SIZE(dmt_timings); i++)
		if (dmt_timings[i].dmt_id == dmt_id)
			return &dmt_timings[i].t;
	return NULL;
}

static const struct short_timings *find_std_id(unsigned short std_id)
{
	unsigned i;

	for (i = 0; i < ARRAY_SIZE(dmt_timings); i++)
		if (dmt_timings[i].std_id == std_id)
			return &dmt_timings[i].t;
	return NULL;
}

static void print_short_timings(const char *prefix, const struct short_timings *t, const char *suffix)
{
	if (!t) {
		// Should not happen
		fail("unknown short timings\n");
		return;
	}
	min_vert_freq_hz = min(min_vert_freq_hz, t->refresh);
	max_vert_freq_hz = max(max_vert_freq_hz, t->refresh);
	min_hor_freq_hz = min(min_hor_freq_hz, t->hor_freq_hz);
	max_hor_freq_hz = max(max_hor_freq_hz, t->hor_freq_hz);
	max_pixclk_khz = max(max_pixclk_khz, t->pixclk_khz);

	printf("%s%ux%u%s@%uHz %s%u:%u HorFreq: %.3f kHz Clock: %.3f MHz%s\n",
	       prefix,
	       t->x, t->y,
	       t->interlaced ? "i" : "",
	       t->refresh,
	       t->rb ? "RB " : "",
	       t->ratio_w, t->ratio_h,
	       t->hor_freq_hz / 1000.0,
	       t->pixclk_khz / 1000.0,
	       suffix);
}

/*
 * Copied from xserver/hw/xfree86/modes/xf86cvt.c
 */
static void edid_cvt_mode(struct short_timings *t, int preferred)
{
	int HDisplay = t->x;
	int VDisplay = t->y;

	/* 1) top/bottom margin size (% of height) - default: 1.8 */
#define CVT_MARGIN_PERCENTAGE 1.8

	/* 2) character cell horizontal granularity (pixels) - default 8 */
#define CVT_H_GRANULARITY 8

	/* 4) Minimum vertical porch (lines) - default 3 */
#define CVT_MIN_V_PORCH 3

	/* 4) Minimum number of vertical back porch lines - default 6 */
#define CVT_MIN_V_BPORCH 6

	/* Pixel Clock step (kHz) */
#define CVT_CLOCK_STEP 250

	float HPeriod;
	unsigned HTotal;
	int VSync;

	/* 2. Horizontal pixels */
	HDisplay = HDisplay - (HDisplay % CVT_H_GRANULARITY);

	/* Determine VSync Width from aspect ratio */
	if (!(VDisplay % 3) && ((VDisplay * 4 / 3) == HDisplay))
		VSync = 4;
	else if (!(VDisplay % 9) && ((VDisplay * 16 / 9) == HDisplay))
		VSync = 5;
	else if (!(VDisplay % 10) && ((VDisplay * 16 / 10) == HDisplay))
		VSync = 6;
	else if (!(VDisplay % 4) && ((VDisplay * 5 / 4) == HDisplay))
		VSync = 7;
	else if (!(VDisplay % 9) && ((VDisplay * 15 / 9) == HDisplay))
		VSync = 7;
	else                        /* Custom */
		VSync = 10;

	if (!t->rb) {             /* simplified GTF calculation */

		/* 4) Minimum time of vertical sync + back porch interval (µs)
		 * default 550.0 */
#define CVT_MIN_VSYNC_BP 550.0

		/* 3) Nominal HSync width (% of line period) - default 8 */
#define CVT_HSYNC_PERCENTAGE 8

		float HBlankPercentage;
		int HBlank;

		/* 8. Estimated Horizontal period */
		HPeriod = ((float) (1000000.0 / t->refresh - CVT_MIN_VSYNC_BP)) /
			(VDisplay + CVT_MIN_V_PORCH);

		/* 5) Definition of Horizontal blanking time limitation */
		/* Gradient (%/kHz) - default 600 */
#define CVT_M_FACTOR 600

		/* Offset (%) - default 40 */
#define CVT_C_FACTOR 40

		/* Blanking time scaling factor - default 128 */
#define CVT_K_FACTOR 128

		/* Scaling factor weighting - default 20 */
#define CVT_J_FACTOR 20

#define CVT_M_PRIME (CVT_M_FACTOR * CVT_K_FACTOR / 256)
#define CVT_C_PRIME ((CVT_C_FACTOR - CVT_J_FACTOR) * CVT_K_FACTOR / 256 + CVT_J_FACTOR)

		/* 12. Find ideal blanking duty cycle from formula */
		HBlankPercentage = CVT_C_PRIME - CVT_M_PRIME * HPeriod / 1000.0;

		/* 13. Blanking time */
		if (HBlankPercentage < 20)
			HBlankPercentage = 20;

		HBlank = HDisplay * HBlankPercentage / (100.0 - HBlankPercentage);
		HBlank -= HBlank % (2 * CVT_H_GRANULARITY);

		/* 14. Find total number of pixels in a line. */
		HTotal = HDisplay + HBlank;
	}
	else {                      /* Reduced blanking */
		/* Minimum vertical blanking interval time (µs) - default 460 */
#define CVT_RB_MIN_VBLANK 460.0

		/* Fixed number of clocks for horizontal sync */
#define CVT_RB_H_SYNC 32.0

		/* Fixed number of clocks for horizontal blanking */
#define CVT_RB_H_BLANK 160.0

		/* Fixed number of lines for vertical front porch - default 3 */
#define CVT_RB_VFPORCH 3

		int VBILines;

		/* 8. Estimate Horizontal period. */
		HPeriod = ((float) (1000000.0 / t->refresh - CVT_RB_MIN_VBLANK)) / VDisplay;

		/* 9. Find number of lines in vertical blanking */
		VBILines = ((float) CVT_RB_MIN_VBLANK) / HPeriod + 1;

		/* 10. Check if vertical blanking is sufficient */
		if (VBILines < (CVT_RB_VFPORCH + VSync + CVT_MIN_V_BPORCH))
			VBILines = CVT_RB_VFPORCH + VSync + CVT_MIN_V_BPORCH;

		/* 12. Find total number of pixels in a line */
		HTotal = HDisplay + CVT_RB_H_BLANK;
	}

	/* 15/13. Find pixel clock frequency (kHz for xf86) */
	t->pixclk_khz = HTotal * 1000.0 / HPeriod;
	t->pixclk_khz -= t->pixclk_khz % CVT_CLOCK_STEP;
	t->hor_freq_hz = (t->pixclk_khz * 1000) / HTotal;

	print_short_timings("    ", t, preferred ? " (preferred vertical rate)" : "");
}

static void detailed_cvt_descriptor(const unsigned char *x, int first)
{
	static const unsigned char empty[3] = { 0, 0, 0 };
	struct short_timings cvt_t = {};
	unsigned char preferred;

	if (!first && !memcmp(x, empty, 3))
		return;

	cvt_t.y = x[0];
	if (!cvt_t.y)
		fail("CVT byte 0 is 0, which is a reserved value\n");
	cvt_t.y |= (x[1] & 0xf0) << 4;
	cvt_t.y++;
	cvt_t.y *= 2;

	switch (x[1] & 0x0c) {
	case 0x00:
	default: /* avoids 'width/ratio may be used uninitialized' warnings */
		cvt_t.ratio_w = 4;
		cvt_t.ratio_h = 3;
		break;
	case 0x04:
		cvt_t.ratio_w = 16;
		cvt_t.ratio_h = 9;
		break;
	case 0x08:
		cvt_t.ratio_w = 16;
		cvt_t.ratio_h = 10;
		break;
	case 0x0c:
		cvt_t.ratio_w = 15;
		cvt_t.ratio_h = 9;
		break;
	}
	cvt_t.x = 8 * (((cvt_t.y * cvt_t.ratio_w) / cvt_t.ratio_h) / 8);

	if (x[1] & 0x03)
		fail("Reserved bits of CVT byte 1 are non-zero\n");
	if (x[2] & 0x80)
		fail("Reserved bit of CVT byte 2 is non-zero\n");
	if (!(x[2] & 0x1f))
		fail("CVT byte 2 does not support any vertical rates\n");
	preferred = (x[2] & 0x60) >> 5;
	if (preferred == 1 && (x[2] & 0x01))
		preferred = 4;
	if (!(x[2] & (1 << (4 - preferred))))
		fail("The preferred CVT Vertical Rate is not supported\n");

	if (x[2] & 0x10) {
		cvt_t.refresh = 50;
		edid_cvt_mode(&cvt_t, preferred == 0);
	}
	if (x[2] & 0x08) {
		cvt_t.refresh = 60;
		edid_cvt_mode(&cvt_t, preferred == 1);
	}
	if (x[2] & 0x04) {
		cvt_t.refresh = 75;
		edid_cvt_mode(&cvt_t, preferred == 2);
	}
	if (x[2] & 0x02) {
		cvt_t.refresh = 85;
		edid_cvt_mode(&cvt_t, preferred == 3);
	}
	if (x[2] & 0x01) {
		cvt_t.refresh = 60;
		cvt_t.rb = 1;
		edid_cvt_mode(&cvt_t, preferred == 4);
	}
}

/* extract a string from a detailed subblock, checking for termination */
static char *extract_string(const unsigned char *x, unsigned len)
{
	static char s[EDID_PAGE_SIZE];
	int seen_newline = 0;
	unsigned i;

	memset(s, 0, sizeof(s));

	for (i = 0; i < len; i++) {
		if (isgraph(x[i])) {
			s[i] = x[i];
		} else if (!seen_newline) {
			if (x[i] == 0x0a) {
				seen_newline = 1;
				if (!i)
					fail("%s: empty string\n", cur_block);
				else if (s[i - 1] == 0x20)
					fail("%s: one or more trailing spaces\n", cur_block);
			} else if (x[i] == 0x20) {
				s[i] = x[i];
			} else {
				fail("%s: non-printable character\n", cur_block);
				return s;
			}
		} else if (x[i] != 0x20) {
			fail("%s: non-space after newline\n", cur_block);
			return s;
		}
	}
	/* Does the string end with a space? */
	if (!seen_newline && s[len - 1] == 0x20)
		fail("%s: one or more trailing spaces\n", cur_block);

	return s;
}

static const struct short_timings established_timings12[] = {
	/* 0x23 bit 7 - 0 */
	{ 720, 400, 70, 9, 5, 31469, 28320 },
	{ 720, 400, 88, 9, 5, 39500, 35500 },
	{ 640, 480, 60, 4, 3, 31469, 25175 },
	{ 640, 480, 67, 4, 3, 35000, 30240 },
	{ 640, 480, 72, 4, 3, 37900, 31500 },
	{ 640, 480, 75, 4, 3, 37500, 31500 },
	{ 800, 600, 56, 4, 3, 35200, 36000 },
	{ 800, 600, 60, 4, 3, 37900, 40000 },
	/* 0x24 bit 7 - 0 */
	{ 800, 600, 72, 4, 3, 48100, 50000 },
	{ 800, 600, 75, 4, 3, 46900, 49500 },
	{ 832, 624, 75, 4, 3, 49726, 57284 },
	{ 1024, 768, 87, 4, 3, 35522, 44900, 0, 1 },
	{ 1024, 768, 60, 4, 3, 48400, 65000 },
	{ 1024, 768, 70, 4, 3, 56500, 75000 },
	{ 1024, 768, 75, 4, 3, 60000, 78750 },
	{ 1280, 1024, 75, 5, 4, 80000, 135000 },
	/* 0x25 bit 7 */
	{ 1152, 870, 75, 192, 145, 67500, 108000 },
};

// The bits in the Established Timings III map to DMT timings,
// this array has the DMT IDs.
static const unsigned char established_timings3_dmt_ids[] = {
	/* 0x06 bit 7 - 0 */
	0x01, // 640x350@85
	0x02, // 640x400@85
	0x03, // 720x400@85
	0x07, // 640x480@85
	0x0e, // 848x480@60
	0x0c, // 800x600@85
	0x13, // 1024x768@85
	0x15, // 1152x864@75
	/* 0x07 bit 7 - 0 */
	0x16, // 1280x768@60 RB
	0x17, // 1280x768@60
	0x18, // 1280x768@75
	0x19, // 1280x768@85
	0x20, // 1280x960@60
	0x21, // 1280x960@85
	0x23, // 1280x1024@60
	0x25, // 1280x1024@85
	/* 0x08 bit 7 - 0 */
	0x27, // 1360x768@60
	0x2e, // 1440x900@60 RB
	0x2f, // 1440x900@60
	0x30, // 1440x900@75
	0x31, // 1440x900@85
	0x29, // 1400x1050@60 RB
	0x2a, // 1400x1050@60
	0x2b, // 1400x1050@75
	/* 0x09 bit 7 - 0 */
	0x2c, // 1400x1050@85
	0x39, // 1680x1050@60 RB
	0x3a, // 1680x1050@60
	0x3b, // 1680x1050@75
	0x3c, // 1680x1050@85
	0x33, // 1600x1200@60
	0x34, // 1600x1200@65
	0x35, // 1600x1200@70
	/* 0x0a bit 7 - 0 */
	0x36, // 1600x1200@75
	0x37, // 1600x1200@85
	0x3e, // 1792x1344@60
	0x3f, // 1792x1344@75
	0x41, // 1856x1392@60
	0x42, // 1856x1392@75
	0x44, // 1920x1200@60 RB
	0x45, // 1920x1200@60
	/* 0x0b bit 7 - 4 */
	0x46, // 1920x1200@75
	0x47, // 1920x1200@85
	0x49, // 1920x1440@60
	0x4a, // 1920x1440@75
};

static void print_standard_timing(uint8_t b1, uint8_t b2)
{
	const struct short_timings *t;
	unsigned ratio_w, ratio_h;
	unsigned x, y, refresh;
	unsigned i;

	if (b1 == 0x01 && b2 == 0x01)
		return;

	if (b1 == 0) {
		fail("non-conformant standard timing (0 horiz)\n");
		return;
	}
	t = find_std_id((b1 << 8) | b2);
	if (t) {
		print_short_timings("  ", t, "");
		return;
	}
	x = (b1 + 31) * 8;
	switch ((b2 >> 6) & 0x3) {
	case 0x00:
		if (claims_one_point_three) {
			y = x * 10 / 16;
			ratio_w = 16;
			ratio_h = 10;
		} else {
			y = x;
			ratio_w = 1;
			ratio_h = 1;
		}
		break;
	case 0x01:
		y = x * 3 / 4;
		ratio_w = 4;
		ratio_h = 3;
		break;
	case 0x02:
		y = x * 4 / 5;
		ratio_w = 5;
		ratio_h = 4;
		break;
	case 0x03:
		y = x * 9 / 16;
		ratio_w = 16;
		ratio_h = 9;
		break;
	}
	refresh = 60 + (b2 & 0x3f);

	min_vert_freq_hz = min(min_vert_freq_hz, refresh);
	max_vert_freq_hz = max(max_vert_freq_hz, refresh);
	for (i = 0; i < ARRAY_SIZE(established_timings12); i++) {
		if (established_timings12[i].x == x &&
		    established_timings12[i].y == y &&
		    established_timings12[i].refresh == refresh &&
		    established_timings12[i].ratio_w == ratio_w &&
		    established_timings12[i].ratio_h == ratio_h) {
			t = &established_timings12[i];
			print_short_timings("  ", t, "");
			return;
		}
	}
	for (i = 0; i < ARRAY_SIZE(dmt_timings); i++) {
		t = &dmt_timings[i].t;

		if (t->x == x && t->y == y &&
		    t->refresh == refresh &&
		    t->ratio_w == ratio_w && t->ratio_h == ratio_h) {
			print_short_timings("  ", t, "");
			return;
		}
	}

	/* TODO: this should also check DMT timings and GTF/CVT */
	printf("  %ux%u@%uHz %u:%u\n",
	       x, y, refresh, ratio_w, ratio_h);
}

/* 1 means valid data */
static int detailed_block(const unsigned char *x, int in_extension)
{
	unsigned ha, hbl, hso, hspw, hborder, va, vbl, vso, vspw, vborder;
	unsigned hor_mm, vert_mm;
	unsigned pixclk_khz;
	double refresh;
	unsigned i;
	char *phsync = "", *pvsync = "";
	char *syncmethod = NULL, *syncmethod_details = "", *stereo;

	if (x[0] == 0 && x[1] == 0) {
		cur_block = "Display Descriptor";
		/* Monitor descriptor block, not detailed timing descriptor. */
		if (x[2] != 0) {
			/* 1.3, 3.10.3 */
			fail("monitor descriptor block has byte 2 nonzero (0x%02x)\n", x[2]);
		}
		if ((!claims_one_point_four || x[3] != 0xfd) && x[4] != 0x00) {
			/* 1.3, 3.10.3 */
			fail("monitor descriptor block has byte 4 nonzero (0x%02x)\n", x[4]);
		}

		seen_non_detailed_descriptor = 1;
		if (x[3] <= 0xf) {
			/*
			 * in principle we can decode these, if we know what they are.
			 * 0x0f seems to be common in laptop panels.
			 * 0x0e is used by EPI: http://www.epi-standard.org/
			 */
			printf("Manufacturer-specified data, tag %d\n", x[3]);
			return 1;
		}
		switch (x[3]) {
		case 0x10:
			cur_block = "Display Descriptor";
			printf("%s\n", cur_block);
			for (i = 5; i < 18; i++) {
				if (x[i]) {
					fail("dummy block filled with garbage\n");
					break;
				}
			}
			return 1;
		case 0xf7:
			cur_block = "Established timings III";
			printf("%s\n", cur_block);
			for (i = 0; i < 44; i++)
				if (x[6 + i / 8] & (1 << (7 - i % 8)))
					print_short_timings("  ", find_dmt_id(established_timings3_dmt_ids[i]), "");
			return 1;
		case 0xf8:
			cur_block = "CVT 3 Byte Timing Codes";
			printf("%s\n", cur_block);
			if (x[5] != 0x01) {
				fail("Invalid version number\n");
				return 0;
			}
			for (i = 0; i < 4; i++)
				detailed_cvt_descriptor(x + 6 + (i * 3), (i == 0));
			return 1;
		case 0xf9:
			cur_block = "Display Color Management Data";
			printf("%s\n", cur_block);
			printf("  Version:  %d\n", x[5]);
			printf("  Red a3:   %.2f\n", (short)(x[6] | (x[7] << 8)) / 100.0);
			printf("  Red a2:   %.2f\n", (short)(x[8] | (x[9] << 8)) / 100.0);
			printf("  Green a3: %.2f\n", (short)(x[10] | (x[11] << 8)) / 100.0);
			printf("  Green a2: %.2f\n", (short)(x[12] | (x[13] << 8)) / 100.0);
			printf("  Blue a3:  %.2f\n", (short)(x[14] | (x[15] << 8)) / 100.0);
			printf("  Blue a2:  %.2f\n", (short)(x[16] | (x[17] << 8)) / 100.0);
			return 1;
		case 0xfa:
			cur_block = "Standard Timing Identifications";
			printf("%s\n", cur_block);
			for (i = 0; i < 6; i++)
				print_standard_timing(x[5 + i * 2], x[5 + i * 2 + 1]);
			return 1;
		case 0xfb: {
			unsigned w_x, w_y;
			unsigned gamma;

			cur_block = "Color Point Data";
			printf("%s\n", cur_block);
			w_x = (x[7] << 2) | ((x[6] >> 2) & 3);
			w_y = (x[8] << 2) | (x[6] & 3);
			gamma = x[9];
			printf("  Index: %u White: 0.%04u, 0.%04u", x[5],
			       (w_x * 10000) / 1024, (w_y * 10000) / 1024);
			if (gamma == 0xff)
				printf(" Gamma: is defined in an extension block");
			else
				printf(" Gamma: %.2f", ((gamma + 100.0) / 100.0));
			printf("\n");
			if (x[10] == 0)
				return 1;
			w_x = (x[12] << 2) | ((x[11] >> 2) & 3);
			w_y = (x[13] << 2) | (x[11] & 3);
			gamma = x[14];
			printf("  Index: %u White: 0.%04u, 0.%04u", x[10],
			       (w_x * 10000) / 1024, (w_y * 10000) / 1024);
			if (gamma == 0xff)
				printf(" Gamma: is defined in an extension block");
			else
				printf(" Gamma: %.2f", ((gamma + 100.0) / 100.0));
			printf("\n");
			return 1;
		}
		case 0xfc:
			cur_block = "Display Product Name";
			has_name_descriptor = 1;
			printf("%s: %s\n", cur_block, extract_string(x + 5, 13));
			return 1;
		case 0xfd: {
			int h_max_offset = 0, h_min_offset = 0;
			int v_max_offset = 0, v_min_offset = 0;
			int is_cvt = 0;
			char *range_class = "";

			cur_block = "Display Range Limits";
			printf("%s\n", cur_block);
			has_range_descriptor = 1;
			/* 
			 * XXX todo: implement feature flags, vtd blocks
			 * XXX check: ranges are well-formed; block termination if no vtd
			 */
			if (claims_one_point_four) {
				if (x[4] & 0x02) {
					v_max_offset = 255;
					if (x[4] & 0x01) {
						v_min_offset = 255;
					}
				}
				if (x[4] & 0x08) {
					h_max_offset = 255;
					if (x[4] & 0x04) {
						h_min_offset = 255;
					}
				}
			}

			/*
			 * despite the values, this is not a bitfield.
			 */
			switch (x[10]) {
			case 0x00: /* default gtf */
				range_class = "GTF";
				break;
			case 0x01: /* range limits only */
				range_class = "bare limits";
				if (!claims_one_point_four)
					fail("'%s' is not allowed for EDID < 1.4\n", range_class);
				break;
			case 0x02: /* secondary gtf curve */
				range_class = "GTF with icing";
				break;
			case 0x04: /* cvt */
				range_class = "CVT";
				is_cvt = 1;
				if (!claims_one_point_four)
					fail("'%s' is not allowed for EDID < 1.4\n", range_class);
				break;
			default: /* invalid */
				fail("invalid range class 0x%02x\n", x[10]);
				range_class = "invalid";
				break;
			}

			if (x[5] + v_min_offset > x[6] + v_max_offset)
				fail("min vertical rate > max vertical rate\n");
			mon_min_vert_freq_hz = x[5] + v_min_offset;
			mon_max_vert_freq_hz = x[6] + v_max_offset;
			if (x[7] + h_min_offset > x[8] + h_max_offset)
				fail("min horizontal freq > max horizontal freq\n");
			mon_min_hor_freq_hz = (x[7] + h_min_offset) * 1000;
			mon_max_hor_freq_hz = (x[8] + h_max_offset) * 1000;
			printf("  Monitor ranges (%s): %d-%dHz V, %d-%dkHz H",
			       range_class,
			       x[5] + v_min_offset, x[6] + v_max_offset,
			       x[7] + h_min_offset, x[8] + h_max_offset);
			if (x[9]) {
				mon_max_pixclk_khz = x[9] * 10000;
				printf(", max dotclock %dMHz\n", x[9] * 10);
			} else {
				if (claims_one_point_four)
					fail("EDID 1.4 block does not set max dotclock\n");
				printf("\n");
			}

			if (is_cvt) {
				int max_h_pixels = 0;

				printf("  CVT version %d.%d\n", (x[11] & 0xf0) >> 4, x[11] & 0x0f);

				if (x[12] & 0xfc) {
					unsigned raw_offset = (x[12] & 0xfc) >> 2;

					printf("  Real max dotclock: %.2fMHz\n",
					       (x[9] * 10) - (raw_offset * 0.25));
					if (raw_offset >= 40)
						warn("CVT block corrects dotclock by more than 9.75MHz\n");
				}

				max_h_pixels = x[12] & 0x03;
				max_h_pixels <<= 8;
				max_h_pixels |= x[13];
				max_h_pixels *= 8;
				if (max_h_pixels)
					printf("  Max active pixels per line: %d\n", max_h_pixels);

				printf("  Supported aspect ratios: %s %s %s %s %s\n",
				       x[14] & 0x80 ? "4:3" : "",
				       x[14] & 0x40 ? "16:9" : "",
				       x[14] & 0x20 ? "16:10" : "",
				       x[14] & 0x10 ? "5:4" : "",
				       x[14] & 0x08 ? "15:9" : "");
				if (x[14] & 0x07)
					fail("Reserved bits of byte 14 are non-zero\n");

				printf("  Preferred aspect ratio: ");
				switch((x[15] & 0xe0) >> 5) {
				case 0x00:
					printf("4:3");
					break;
				case 0x01:
					printf("16:9");
					break;
				case 0x02:
					printf("16:10");
					break;
				case 0x03:
					printf("5:4");
					break;
				case 0x04:
					printf("15:9");
					break;
				default:
					printf("(broken)");
					fail("invalid preferred aspect ratio\n");
					break;
				}
				printf("\n");

				if (x[15] & 0x08)
					printf("  Supports CVT standard blanking\n");
				if (x[15] & 0x10)
					printf("  Supports CVT reduced blanking\n");

				if (x[15] & 0x07)
					fail("Reserved bits of byte 15 are non-zero\n");

				if (x[16] & 0xf0) {
					printf("  Supported display scaling:\n");
					if (x[16] & 0x80)
						printf("    Horizontal shrink\n");
					if (x[16] & 0x40)
						printf("    Horizontal stretch\n");
					if (x[16] & 0x20)
						printf("    Vertical shrink\n");
					if (x[16] & 0x10)
						printf("    Vertical stretch\n");
				}

				if (x[16] & 0x0f)
					fail("Reserved bits of byte 16 are non-zero\n");

				if (x[17])
					printf("  Preferred vertical refresh: %d Hz\n", x[17]);
				else
					warn("CVT block does not set preferred refresh rate\n");
			}

			/*
			 * Slightly weird to return a global, but I've never seen any
			 * EDID block wth two range descriptors, so it's harmless.
			 */
			return 0;
		}
		case 0xfe:
			/*
			 * TODO: Two of these in a row, in the third and fourth slots,
			 * seems to be specified by SPWG: http://www.spwg.org/
			 */
			cur_block = "Alphanumeric Data String";
			printf("%s: %s\n", cur_block,
			       extract_string(x + 5, 13));
			return 1;
		case 0xff:
			cur_block = "Display Product Serial Number";
			printf("%s: %s\n", cur_block,
			       extract_string(x + 5, 13));
			has_serial_string = 1;
			return 1;
		default:
			printf("Unknown monitor description type %d\n", x[3]);
			return 0;
		}
	}

	if (seen_non_detailed_descriptor && !in_extension) {
		has_valid_descriptor_ordering = 0;
	}

	cur_block = "Detailed Timings";
	did_detailed_timing = 1;
	ha = (x[2] + ((x[4] & 0xf0) << 4));
	hbl = (x[3] + ((x[4] & 0x0f) << 8));
	hso = (x[8] + ((x[11] & 0xc0) << 2));
	hspw = (x[9] + ((x[11] & 0x30) << 4));
	hborder = x[15];
	va = (x[5] + ((x[7] & 0xf0) << 4));
	vbl = (x[6] + ((x[7] & 0x0f) << 8));
	vso = ((x[10] >> 4) + ((x[11] & 0x0c) << 2));
	vspw = ((x[10] & 0x0f) + ((x[11] & 0x03) << 4));
	vborder = x[16];
	switch ((x[17] & 0x18) >> 3) {
	case 0x00:
		syncmethod = "analog composite";
		/* fall-through */
	case 0x01:
		if (!syncmethod)
			syncmethod = "bipolar analog composite";
		switch ((x[17] & 0x06) >> 1) {
		case 0x00:
			syncmethod_details = ", sync-on-green";
			break;
		case 0x01:
			break;
		case 0x02:
			syncmethod_details = ", serrate, sync-on-green";
			break;
		case 0x03:
			syncmethod_details = ", serrate";
			break;
		}
		break;
	case 0x02:
		syncmethod = "digital composite";
		phsync = (x[17] & (1 << 1)) ? "+hsync " : "-hsync ";
		if (x[17] & (1 << 2))
		    syncmethod_details = ", serrate";
		break;
	case 0x03:
		syncmethod = "";
		pvsync = (x[17] & (1 << 2)) ? "+vsync " : "-vsync ";
		phsync = (x[17] & (1 << 1)) ? "+hsync " : "-hsync ";
		break;
	}
	switch (x[17] & 0x61) {
	case 0x20:
		stereo = "field sequential L/R";
		break;
	case 0x40:
		stereo = "field sequential R/L";
		break;
	case 0x21:
		stereo = "interleaved right even";
		break;
	case 0x41:
		stereo = "interleaved left even";
		break;
	case 0x60:
		stereo = "four way interleaved";
		break;
	case 0x61:
		stereo = "side by side interleaved";
		break;
	default:
		stereo = "";
		break;
	}

	if (!ha || !hbl || !hso || !hspw || !va || !vbl || !vso || !vspw)
		fail("0 values in the detailed timings:\n"
		     "  Horizontal Active/Blanking %u/%u\n"
		     "  Horizontal Sync Offset/Width %u/%u\n"
		     "  Vertical Active/Blanking %u/%u\n"
		     "  Vertical Sync Offset/Width %u/%u\n",
		     ha, hbl, hso, hspw, va, vbl, vso, vspw);

	pixclk_khz = (x[0] + (x[1] << 8)) * 10;
	if (pixclk_khz < 10000)
		fail("pixelclock < 10 MHz\n");
	if ((ha + hbl) && (va + vbl))
		refresh = (pixclk_khz * 1000.0) / ((ha + hbl) * (va + vbl));
	else
		refresh = 0.0;
	hor_mm = x[12] + ((x[14] & 0xf0) << 4);
	vert_mm = x[13] + ((x[14] & 0x0f) << 8);
	printf("Detailed mode: Clock %.3f MHz, %u mm x %u mm\n"
	       "               %4u %4u %4u %4u (%3u %3u %3d) hborder %u\n"
	       "               %4u %4u %4u %4u (%3u %3u %3d) vborder %u\n"
	       "               %s%s%s%s%s%s%s\n"
	       "               VertFreq: %.3f Hz, HorFreq: %.3f kHz\n",
	       pixclk_khz / 1000.0,
	       hor_mm, vert_mm,
	       ha, ha + hso, ha + hso + hspw, ha + hbl, hso, hspw, hbl - hso - hspw, hborder,
	       va, va + vso, va + vso + vspw, va + vbl, vso, vspw, vbl - vso - vspw, vborder,
	       phsync, pvsync, syncmethod, syncmethod_details,
	       syncmethod && ((x[17] & 0x80) || *stereo) ? ", " : "",
	       x[17] & 0x80 ? "interlaced " : "", stereo,
	       refresh, ha + hbl ? (double)pixclk_khz / (ha + hbl) : 0.0);
	if (hso + hspw >= hbl)
		fail("0 or negative horizontal back porch\n");
	if (vso + vspw >= vbl)
		fail("0 or negative vertical back porch\n");
	if ((!max_display_width_mm && hor_mm) ||
	    (!max_display_height_mm && vert_mm)) {
		fail("mismatch of image size vs display size: image size is set, but not display size\n");
	} else if ((max_display_width_mm && !hor_mm) ||
		   (max_display_height_mm && !vert_mm)) {
		fail("mismatch of image size vs display size: image size is not set, but display size is\n");
	} else if (!hor_mm && !vert_mm) {
		/* this is valid */
	} else if (hor_mm > max_display_width_mm + 9 ||
		   vert_mm > max_display_height_mm + 9) {
		fail("mismatch of image size %ux%u mm vs display size %ux%u mm\n",
		     hor_mm, vert_mm, max_display_width_mm, max_display_height_mm);
	} else if (hor_mm < max_display_width_mm - 9 &&
		   vert_mm < max_display_height_mm - 9) {
		fail("mismatch of image size %ux%u mm vs display size %ux%u mm\n",
		     hor_mm, vert_mm, max_display_width_mm, max_display_height_mm);
	}
	if (refresh) {
		min_vert_freq_hz = min(min_vert_freq_hz, refresh);
		max_vert_freq_hz = max(max_vert_freq_hz, refresh);
	}
	if (pixclk_khz && (ha + hbl)) {
		min_hor_freq_hz = min(min_hor_freq_hz, (pixclk_khz * 1000) / (ha + hbl));
		max_hor_freq_hz = max(max_hor_freq_hz, (pixclk_khz * 1000) / (ha + hbl));
		max_pixclk_khz = max(max_pixclk_khz, pixclk_khz);
	}

	return 1;
}

static int do_checksum(const unsigned char *x, size_t len)
{
	unsigned char check = x[len - 1];
	unsigned char sum = 0;
	unsigned i;

	printf("Checksum: 0x%hhx", check);

	for (i = 0; i < len-1; i++)
		sum += x[i];

	if ((unsigned char)(check + sum) != 0) {
		printf(" (should be 0x%x)\n", -sum & 0xff);
		return 0;
	}

	printf(" (valid)\n");
	return 1;
}

/* CTA extension */

static const char *audio_ext_format(unsigned char x)
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
	default: return "RESERVED";
	}
	return "BROKEN"; /* can't happen */
}

static const char *audio_format(unsigned char x)
{
	switch (x) {
	case 0: return "RESERVED";
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
	case 15: return "RESERVED";
	}
	return "BROKEN"; /* can't happen */
}

static const char *mpeg_h_3d_audio_level(unsigned char x)
{
	switch (x) {
	case 0: return "Unspecified";
	case 1: return "Level 1";
	case 2: return "Level 2";
	case 3: return "Level 3";
	case 4: return "Level 4";
	case 5: return "Level 5";
	case 6: return "Reserved";
	case 7: return "Reserved";
	}
	return "BROKEN"; /* can't happen */
}

static void cta_audio_block(const unsigned char *x, unsigned length)
{
	unsigned i, format, ext_format = 0;

	if (length % 3) {
		printf("Broken CTA audio block length %d\n", length);
		/* XXX non-conformant */
		return;
	}

	for (i = 0; i < length; i += 3) {
		format = (x[i] & 0x78) >> 3;
		ext_format = (x[i + 2] & 0xf8) >> 3;
		if (format != 15)
			printf("    %s, max channels %u\n", audio_format(format),
			       (x[i] & 0x07)+1);
		else if (ext_format == 11)
			printf("    %s, MPEG-H 3D Audio Level: %s\n", audio_ext_format(ext_format),
			       mpeg_h_3d_audio_level(x[i] & 0x07));
		else if (ext_format == 13)
			printf("    %s, max channels %u\n", audio_ext_format(ext_format),
			       (((x[i + 1] & 0x80) >> 3) | ((x[i] & 0x80) >> 4) |
				(x[i] & 0x07))+1);
		else
			printf("    %s, max channels %u\n", audio_ext_format(ext_format),
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

struct edid_cta_mode {
	const char *name;
	unsigned refresh, hor_freq_hz, pixclk_khz;
};

static struct edid_cta_mode edid_cta_modes1[] = {
	/* VIC 1 */
	{ "640x480@60Hz 4:3", 60, 31469, 25175 },
	{ "720x480@60Hz 4:3", 60, 31469, 27000 },
	{ "720x480@60Hz 16:9", 60, 31469, 27000 },
	{ "1280x720@60Hz 16:9", 60, 45000, 74250 },
	{ "1920x1080i@60Hz 16:9", 60, 33750, 74250 },
	{ "1440x480i@60Hz 4:3", 60, 15734, 27000 },
	{ "1440x480i@60Hz 16:9", 60, 15734, 27000 },
	{ "1440x240@60Hz 4:3", 60, 15734, 27000 },
	{ "1440x240@60Hz 16:9", 60, 15734, 27000 },
	{ "2880x480i@60Hz 4:3", 60, 15734, 54000 },
	/* VIC 11 */
	{ "2880x480i@60Hz 16:9", 60, 15734, 54000 },
	{ "2880x240@60Hz 4:3", 60, 15734, 54000 },
	{ "2880x240@60Hz 16:9", 60, 15734, 54000 },
	{ "1440x480@60Hz 4:3", 60, 31469, 54000 },
	{ "1440x480@60Hz 16:9", 60, 31469, 54000 },
	{ "1920x1080@60Hz 16:9", 60, 67500, 148500 },
	{ "720x576@50Hz 4:3", 50, 31250, 27000 },
	{ "720x576@50Hz 16:9", 50, 31250, 27000 },
	{ "1280x720@50Hz 16:9", 50, 37500, 74250 },
	{ "1920x1080i@50Hz 16:9", 50, 28125, 74250 },
	/* VIC 21 */
	{ "1440x576i@50Hz 4:3", 50, 15625, 27000 },
	{ "1440x576i@50Hz 16:9", 50, 15625, 27000 },
	{ "1440x288@50Hz 4:3", 50, 15625, 27000 },
	{ "1440x288@50Hz 16:9", 50, 15625, 27000 },
	{ "2880x576i@50Hz 4:3", 50, 15625, 54000 },
	{ "2880x576i@50Hz 16:9", 50, 15625, 54000 },
	{ "2880x288@50Hz 4:3", 50, 15625, 54000 },
	{ "2880x288@50Hz 16:9", 50, 15625, 54000 },
	{ "1440x576@50Hz 4:3", 50, 31250, 54000 },
	{ "1440x576@50Hz 16:9", 50, 31250, 54000 },
	/* VIC 31 */
	{ "1920x1080@50Hz 16:9", 50, 56250, 148500 },
	{ "1920x1080@24Hz 16:9", 24, 27000, 74250 },
	{ "1920x1080@25Hz 16:9", 25, 28125, 74250 },
	{ "1920x1080@30Hz 16:9", 30, 33750, 74250 },
	{ "2880x480@60Hz 4:3", 60, 31469, 108000 },
	{ "2880x480@60Hz 16:9", 60, 31469, 108000 },
	{ "2880x576@50Hz 4:3", 50, 31250, 108000 },
	{ "2880x576@50Hz 16:9", 50, 31250, 108000 },
	{ "1920x1080i@50Hz 16:9", 50, 31250, 72000 },
	{ "1920x1080i@100Hz 16:9", 100, 56250, 148500 },
	/* VIC 41 */
	{ "1280x720@100Hz 16:9", 100, 75000, 148500 },
	{ "720x576@100Hz 4:3", 100, 62500, 54000 },
	{ "720x576@100Hz 16:9", 100, 62500, 54000 },
	{ "1440x576i@100Hz 4:3", 100, 31250, 54000 },
	{ "1440x576i@100Hz 16:9", 100, 31250, 54000 },
	{ "1920x1080i@120Hz 16:9", 120, 67500, 148500 },
	{ "1280x720@120Hz 16:9", 120, 90000, 148500 },
	{ "720x480@120Hz 4:3", 120, 62937, 54000 },
	{ "720x480@120Hz 16:9", 120, 62937, 54000 },
	{ "1440x480i@120Hz 4:3", 120, 31469, 54000 },
	/* VIC 51 */
	{ "1440x480i@120Hz 16:9", 120, 31469, 54000 },
	{ "720x576@200Hz 4:3", 200, 125000, 108000 },
	{ "720x576@200Hz 16:9", 200, 125000, 108000 },
	{ "1440x576i@200Hz 4:3", 200, 62500, 108000 },
	{ "1440x576i@200Hz 16:9", 200, 62500, 108000 },
	{ "720x480@240Hz 4:3", 240, 125874, 108000 },
	{ "720x480@240Hz 16:9", 240, 125874, 108000 },
	{ "1440x480i@240Hz 4:3", 240, 62937, 108000 },
	{ "1440x480i@240Hz 16:9", 240, 62937, 108000 },
	{ "1280x720@24Hz 16:9", 24, 18000, 59400 },
	/* VIC 61 */
	{ "1280x720@25Hz 16:9", 25, 18750, 74250 },
	{ "1280x720@30Hz 16:9", 30, 22500, 74250 },
	{ "1920x1080@120Hz 16:9", 120, 135000, 297000 },
	{ "1920x1080@100Hz 16:9", 100, 112500, 297000 },
	{ "1280x720@24Hz 64:27", 24, 18000, 59400 },
	{ "1280x720@25Hz 64:27", 25, 18750, 74250 },
	{ "1280x720@30Hz 64:27", 30, 22500, 74250 },
	{ "1280x720@50Hz 64:27", 50, 37500, 74250 },
	{ "1280x720@60Hz 64:27", 60, 45000, 74250 },
	{ "1280x720@100Hz 64:27", 100, 75000, 148500 },
	/* VIC 71 */
	{ "1280x720@120Hz 64:27", 120, 91000, 148500 },
	{ "1920x1080@24Hz 64:27", 24, 27000, 74250 },
	{ "1920x1080@25Hz 64:27", 25, 28125, 74250 },
	{ "1920x1080@30Hz 64:27", 30, 33750, 74250 },
	{ "1920x1080@50Hz 64:27", 50, 56250, 148500 },
	{ "1920x1080@60Hz 64:27", 60, 67500, 148500 },
	{ "1920x1080@100Hz 64:27", 100, 112500, 297000 },
	{ "1920x1080@120Hz 64:27", 120, 135000, 297000 },
	{ "1680x720@24Hz 64:27", 24, 18000, 59400 },
	{ "1680x720@25Hz 64:27", 25, 18750, 59400 },
	/* VIC 81 */
	{ "1680x720@30Hz 64:27", 30, 22500, 59400 },
	{ "1680x720@50Hz 64:27", 50, 37500, 82500 },
	{ "1680x720@60Hz 64:27", 60, 45000, 99000 },
	{ "1680x720@100Hz 64:27", 100, 82500, 165000 },
	{ "1680x720@120Hz 64:27", 120, 99000, 198000 },
	{ "2560x1080@24Hz 64:27", 24, 26400, 99000 },
	{ "2560x1080@25Hz 64:27", 25, 28125, 90000 },
	{ "2560x1080@30Hz 64:27", 30, 33750, 118800 },
	{ "2560x1080@50Hz 64:27", 50, 56250, 185625 },
	{ "2560x1080@60Hz 64:27", 60, 66000, 198000 },
	/* VIC 91 */
	{ "2560x1080@100Hz 64:27", 100, 125000, 371250 },
	{ "2560x1080@120Hz 64:27", 120, 150000, 495000 },
	{ "3840x2160@24Hz 16:9", 24, 54000, 297000 },
	{ "3840x2160@25Hz 16:9", 25, 56250, 297000 },
	{ "3840x2160@30Hz 16:9", 30, 67500, 297000 },
	{ "3840x2160@50Hz 16:9", 50, 112500, 594000 },
	{ "3840x2160@60Hz 16:9", 60, 135000, 594000 },
	{ "4096x2160@24Hz 256:135", 24, 54000, 297000 },
	{ "4096x2160@25Hz 256:135", 25, 56250, 297000 },
	{ "4096x2160@30Hz 256:135", 30, 67500, 297000 },
	/* VIC 101 */
	{ "4096x2160@50Hz 256:135", 50, 112500, 594000 },
	{ "4096x2160@60Hz 256:135", 60, 135000, 594000 },
	{ "3840x2160@24Hz 64:27", 24, 54000, 297000 },
	{ "3840x2160@25Hz 64:27", 25, 56250, 297000 },
	{ "3840x2160@30Hz 64:27", 30, 67500, 297000 },
	{ "3840x2160@50Hz 64:27", 50, 112500, 594000 },
	{ "3840x2160@60Hz 64:27", 60, 135000, 594000 },
	{ "1280x720@48Hz 16:9", 48, 36000, 90000 },
	{ "1280x720@48Hz 64:27", 48, 36000, 90000 },
	{ "1680x720@48Hz 64:27", 48, 36000, 99000 },
	/* VIC 111 */
	{ "1920x1080@48Hz 16:9", 48, 54000, 148500 },
	{ "1920x1080@48Hz 64:27", 48, 54000, 148500 },
	{ "2560x1080@48Hz 64:27", 48, 52800, 198000 },
	{ "3840x2160@48Hz 16:9", 48, 108000, 594000 },
	{ "4096x2160@48Hz 256:135", 48, 108000, 594000 },
	{ "3840x2160@48Hz 64:27", 48, 108000, 594000 },
	{ "3840x2160@100Hz 16:9", 100, 225000, 1188000 },
	{ "3840x2160@120Hz 16:9", 120, 270000, 1188000 },
	{ "3840x2160@100Hz 64:27", 100, 225000, 1188000 },
	{ "3840x2160@120Hz 64:27", 120, 270000, 1188000 },
	/* VIC 121 */
	{ "5120x2160@24Hz 64:27", 24, 52800, 396000 },
	{ "5120x2160@25Hz 64:27", 25, 55000, 396000 },
	{ "5120x2160@30Hz 64:27", 30, 66000, 396000 },
	{ "5120x2160@48Hz 64:27", 48, 118800, 742500 },
	{ "5120x2160@50Hz 64:27", 50, 112500, 742500 },
	{ "5120x2160@60Hz 64:27", 60, 135000, 742500 },
	{ "5120x2160@100Hz 64:27", 100, 225000, 1485000 },
};

static struct edid_cta_mode edid_cta_modes2[] = {
	/* VIC 193 */
	{ "5120x2160@120Hz 64:27", 120, 270000, 1485000 },
	{ "7680x4320@24Hz 16:9", 24, 108000, 1188000 },
	{ "7680x4320@25Hz 16:9", 25, 110000, 1188000 },
	{ "7680x4320@30Hz 16:9", 30, 132000, 1188000 },
	{ "7680x4320@48Hz 16:9", 48, 216000, 2376000 },
	{ "7680x4320@50Hz 16:9", 50, 220000, 2376000 },
	{ "7680x4320@60Hz 16:9", 60, 264000, 2376000 },
	{ "7680x4320@100Hz 16:9", 100, 450000, 4752000 },
	/* VIC 201 */
	{ "7680x4320@120Hz 16:9", 120, 540000, 4752000 },
	{ "7680x4320@24Hz 64:27", 24, 108000, 1188000 },
	{ "7680x4320@25Hz 64:27", 25, 110000, 1188000 },
	{ "7680x4320@30Hz 64:27", 30, 132000, 1188000 },
	{ "7680x4320@48Hz 64:27", 48, 216000, 2376000 },
	{ "7680x4320@50Hz 64:27", 50, 220000, 2376000 },
	{ "7680x4320@60Hz 64:27", 60, 264000, 2376000 },
	{ "7680x4320@100Hz 64:27", 100, 450000, 4752000 },
	{ "7680x4320@120Hz 64:27", 120, 540000, 4752000 },
	{ "10240x4320@24Hz 64:27", 24, 118800, 1485000 },
	/* VIC 211 */
	{ "10240x4320@25Hz 64:27", 25, 110000, 1485000 },
	{ "10240x4320@30Hz 64:27", 30, 135000, 1485000 },
	{ "10240x4320@48Hz 64:27", 48, 237600, 2970000 },
	{ "10240x4320@50Hz 64:27", 50, 220000, 2970000 },
	{ "10240x4320@60Hz 64:27", 60, 270000, 2970000 },
	{ "10240x4320@100Hz 64:27", 100, 450000, 5940000 },
	{ "10240x4320@120Hz 64:27", 120, 540000, 5940000 },
	{ "4096x2160@100Hz 256:135", 100, 225000, 1188000 },
	{ "4096x2160@120Hz 256:135", 120, 270000, 1188000 },
};

static const struct edid_cta_mode *vic_to_mode(unsigned char vic)
{
	if (vic > 0 && vic <= ARRAY_SIZE(edid_cta_modes1))
		return edid_cta_modes1 + vic - 1;
	if (vic >= 193 && vic <= ARRAY_SIZE(edid_cta_modes2) + 193)
		return edid_cta_modes2 + vic - 193;
	return NULL;
}

static void cta_svd(const unsigned char *x, unsigned n, int for_ycbcr420)
{
	unsigned i;

	for (i = 0; i < n; i++)  {
		const struct edid_cta_mode *vicmode = NULL;
		unsigned char svd = x[i];
		unsigned char native;
		unsigned char vic;
		const char *mode;
		unsigned hfreq = 0;
		unsigned clock_khz = 0;

		if ((svd & 0x7f) == 0)
			continue;

		if ((svd - 1) & 0x40) {
			vic = svd;
			native = 0;
		} else {
			vic = svd & 0x7f;
			native = svd & 0x80;
		}

		vicmode = vic_to_mode(vic);
		if (vicmode) {
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
			mode = vicmode->name;
			min_vert_freq_hz = min(min_vert_freq_hz, vicmode->refresh);
			max_vert_freq_hz = max(max_vert_freq_hz, vicmode->refresh);
			hfreq = vicmode->hor_freq_hz;
			min_hor_freq_hz = min(min_hor_freq_hz, hfreq);
			max_hor_freq_hz = max(max_hor_freq_hz, hfreq);
			clock_khz = vicmode->pixclk_khz / (for_ycbcr420 ? 2 : 1);
			max_pixclk_khz = max(max_pixclk_khz, clock_khz);
		} else {
			mode = "Unknown mode";
		}

		printf("    VIC %3u %s%sHorFreq: %.3f kHz Clock: %.3f MHz\n",
		       vic, mode, native ? " (native) " : " ", hfreq / 1000.0, clock_khz / 1000.0);
		if (vic == 1)
			has_cta861_vic_1 = 1;
	}
}

static void cta_video_block(const unsigned char *x, unsigned length)
{
	cta_svd(x, length, 0);
}

static void cta_y420vdb(const unsigned char *x, unsigned length)
{
	cta_svd(x, length, 1);
}

static void cta_y420cmdb(const unsigned char *x, unsigned length)
{
	unsigned i;

	for (i = 0; i < length; i++) {
		uint8_t v = x[0 + i];
		unsigned j;

		for (j = 0; j < 8; j++)
			if (v & (1 << j))
				printf("    VSD Index %u\n", i * 8 + j);
	}
}

static void cta_vfpdb(const unsigned char *x, unsigned length)
{
	unsigned i;

	for (i = 0; i < length; i++)  {
		unsigned char svr = x[i];

		if ((svr > 0 && svr < 128) || (svr > 192 && svr < 254)) {
			const struct edid_cta_mode *vicmode;
			unsigned char vic;
			const char *mode;

			vic = svr;

			vicmode = vic_to_mode(vic);
			if (vicmode)
				mode = vicmode->name;
			else
				mode = "Unknown mode";

			printf("    VIC %02u %s\n", vic, mode);

		} else if (svr > 128 && svr < 145) {
			printf("    DTD number %02u\n", svr - 128);
		}
	}
}

static struct {
	const char *name;
	unsigned refresh, hor_freq_hz, pixclk_khz;
} edid_hdmi_modes[] = {
	{"3840x2160@30Hz 16:9", 30, 67500, 297000},
	{"3840x2160@25Hz 16:9", 25, 56250, 297000},
	{"3840x2160@24Hz 16:9", 24, 54000, 297000},
	{"4096x2160@24Hz 256:135", 24, 54000, 297000},
};

static void cta_hdmi_block(const unsigned char *x, unsigned length)
{
	int mask = 0, formats = 0;
	int len_vic, len_3d;
	int b = 0;

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
	/* two reserved */
	if (x[5] & 0x01)
		printf("    DVI_Dual\n");

	if (length < 7)
		return;

	printf("    Maximum TMDS clock: %uMHz\n", x[6] * 5);
	if (x[6] * 5 > 340)
		nonconformant_hdmi_vsdb_tmds_rate = 1;

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
		unsigned hfreq = 0;
		unsigned clock_khz = 0;
		unsigned i;

		for (i = 0; i < len_vic; i++) {
			unsigned char vic = x[8 + b + i];
			const char *mode;

			if (vic && vic <= ARRAY_SIZE(edid_hdmi_modes)) {
				supported_hdmi_vic_codes |= 1 << (vic - 1);
				mode = edid_hdmi_modes[vic - 1].name;
				min_vert_freq_hz = min(min_vert_freq_hz, edid_hdmi_modes[vic - 1].refresh);
				max_vert_freq_hz = max(max_vert_freq_hz, edid_hdmi_modes[vic - 1].refresh);
				hfreq = edid_hdmi_modes[vic - 1].hor_freq_hz;
				min_hor_freq_hz = min(min_hor_freq_hz, hfreq);
				max_hor_freq_hz = max(max_hor_freq_hz, hfreq);
				clock_khz = edid_hdmi_modes[vic - 1].pixclk_khz;
				max_pixclk_khz = max(max_pixclk_khz, clock_khz);
			} else {
				mode = "Unknown mode";
			}

			printf("      HDMI VIC %u %s HorFreq: %.3f kHz Clock: %.3f MHz\n",
			       vic, mode, hfreq / 1000.0, clock_khz / 1000.0);
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
		nonconformant_hf_eeodb = 1;
}

static void cta_hf_scdb(const unsigned char *x, unsigned length)
{
	unsigned rate = x[1] * 5;

	printf("    Version: %u\n", x[0]);
	if (rate) {
		printf("    Maximum TMDS Character Rate: %uMHz\n", rate);
		if ((rate && rate <= 340) || rate > 600)
			nonconformant_hf_vsdb_tmds_rate = 1;
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
		if (max_frl_rate >= ARRAY_SIZE(max_frl_rates))
			printf("Reserved\n");
		else
			printf("%s\n", max_frl_rates[max_frl_rate]);
		if (max_frl_rate == 1 && rate < 300)
			nonconformant_hf_vsdb_tmds_rate = 1;
		else if (max_frl_rate >= 2 && rate < 600)
			nonconformant_hf_vsdb_tmds_rate = 1;
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

		if (max_slices < ARRAY_SIZE(dsc_max_slices))
			printf("    Supports %s\n", dsc_max_slices[max_slices]);
	}
	if (x[8] & 0xf0) {
		unsigned max_frl_rate = x[8] >> 4;

		printf("    DSC Max Fixed Rate Link: ");
		if (max_frl_rate >= ARRAY_SIZE(max_frl_rates))
			printf("Reserved\n");
		else
			printf("%s\n", max_frl_rates[max_frl_rate]);
	}
	if (x[9] & 0x3f)
		printf("    Maximum number of bytes in a line of chunks: %u\n",
		       1024 * (1 + (x[9] & 0x3f)));
}

static void cta_hdr10plus(const unsigned char *x, unsigned length)
{
	printf("    Application Version: %u\n", x[0]);
}

static void hex_block(const char *prefix, const unsigned char *x, unsigned length)
{
	unsigned i, j;

	if (!length)
		return;

	for (i = 0; i < length; i += 16) {
		printf("%s", prefix);
		for (j = 0; j < 16; j++)
			if (i + j < length)
				printf("%02x ", x[i + j]);
			else if (length > 16)
				printf("   ");
		for (j = 0; j < 16 && i + j < length; j++)
			printf("%c", x[i + j] >= ' ' && x[i + j] <= '~' ? x[i + j] : '.');
		printf("\n");
	}
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
	uint32_t sad;
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
	uint32_t spm = ((x[3] << 16) | (x[2] << 8) | x[1]);
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
	int i;

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
				printf("      %s\n", i < ARRAY_SIZE(eotf_map) ?
				       eotf_map[i] : "Unknown");
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
			printf("    InfoFrame Type Code %u IEEE OUI: %02x%02x%02x\n",
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

			printf("    %s, max channels %u\n", audio_format(format),
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
			uint32_t sad = ((x[2] << 16) | (x[1] << 8) | x[0]);
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
				printf("    Unknown Speaker Allocation (%d)\n", x[3] >> 4);
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

static void cta_block(const unsigned char *x)
{
	static int last_block_was_hdmi_vsdb;
	static int have_hf_vsdb, have_hf_scdb;
	static int first_block = 1;
	unsigned length = x[0] & 0x1f;
	unsigned oui;

	switch ((x[0] & 0xe0) >> 5) {
	case 0x01:
		cur_block = "Audio Data Block";
		printf("  Audio Data Block\n");
		cta_audio_block(x + 1, length);
		break;
	case 0x02:
		cur_block = "Video Data Block";
		printf("  Video Data Block\n");
		cta_video_block(x + 1, length);
		break;
	case 0x03:
		oui = (x[3] << 16) + (x[2] << 8) + x[1];
		printf("  Vendor-Specific Data Block, OUI %06x", oui);
		if (oui == 0x000c03) {
			cur_block = "Vendor-Specific Data Block (HDMI)";
			cta_hdmi_block(x + 1, length);
			last_block_was_hdmi_vsdb = 1;
			first_block = 0;
			return;
		}
		if (oui == 0xc45dd8) {
			cur_block = "Vendor-Specific Data Block (HDMI Forum)";
			if (!last_block_was_hdmi_vsdb)
				nonconformant_hf_vsdb_position = 1;
			if (have_hf_scdb || have_hf_vsdb)
				duplicate_scdb = 1;
			printf(" (HDMI Forum)\n");
			cta_hf_scdb(x + 4, length - 3);
			have_hf_vsdb = 1;
		} else {
			printf(" (unknown)\n");
			hex_block("    ", x + 4, length - 3);
		}
		break;
	case 0x04:
		cur_block = "Speaker Allocation Data Block";
		printf("  Speaker Allocation Data Block\n");
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
			cur_block = "Video Capability Data Block";
			printf("Video Capability Data Block\n");
			cta_vcdb(x + 2, length - 1);
			break;
		case 0x01:
			oui = (x[4] << 16) + (x[3] << 8) + x[2];
			printf("Vendor-Specific Video Data Block, OUI %06x", oui);
			if (oui == 0x90848b) {
				cur_block = "Vendor-Specific Video Data Block (HDR10+)";
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
			cur_block = "Colorimetry Data Block";
			printf("Colorimetry Data Block\n");
			cta_colorimetry_block(x + 2, length - 1);
			break;
		case 0x06:
			cur_block = "HDR Static Metadata Data Block";
			printf("HDR Static Metadata Data Block\n");
			cta_hdr_static_metadata_block(x + 2, length - 1);
			break;
		case 0x07:
			cur_block = "HDR Dynamic Metadata Data Block";
			printf("HDR Dynamic Metadata Data Block\n");
			cta_hdr_dyn_metadata_block(x + 2, length - 1);
			break;
		case 0x0d:
			cur_block = "Video Format Preference Data Block";
			printf("Video Format Preference Data Block\n");
			cta_vfpdb(x + 2, length - 1);
			break;
		case 0x0e:
			cur_block = "YCbCr 4:2:0 Video Data Block";
			printf("YCbCr 4:2:0 Video Data Block\n");
			cta_y420vdb(x + 2, length - 1);
			break;
		case 0x0f:
			cur_block = "YCbCr 4:2:0 Capability Map Data Block";
			printf("YCbCr 4:2:0 Capability Map Data Block\n");
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
			cur_block = "HDMI Audio Data Block";
			printf("HDMI Audio Data Block\n");
			cta_hdmi_audio_block(x + 2, length - 1);
			break;
		case 0x13:
			cur_block = "Room Configuration Data Block";
			printf("Room Configuration Data Block\n");
			cta_rcdb(x + 2, length - 1);
			break;
		case 0x14:
			cur_block = "Speaker Location Data Block";
			printf("Speaker Location Data Block\n");
			cta_sldb(x + 2, length - 1);
			break;
		case 0x20:
			printf("InfoFrame Data Block\n");
			cta_ifdb(x + 2, length - 1);
			break;
		case 0x78:
			cur_block = "HDMI Forum EDID Extension Override Data Block";
			printf("HDMI Forum EDID Extension Override Data Block\n");
			cta_hf_eeodb(x + 2, length - 1);
			// This must be the first CTA block
			if (!first_block)
				nonconformant_hf_eeodb = 1;
			break;
		case 0x79:
			cur_block = "HDMI Forum Sink Capability Data Block";
			printf("HDMI Forum Sink Capability Data Block\n");
			if (!last_block_was_hdmi_vsdb)
				nonconformant_hf_vsdb_position = 1;
			if (have_hf_scdb || have_hf_vsdb)
				duplicate_scdb = 1;
			if (x[2] || x[3])
				printf("  Non-zero SCDB reserved fields!\n");
			cta_hf_scdb(x + 4, length - 3);
			have_hf_scdb = 1;
			break;
		default:
			if (x[1] >= 6 && x[1] <= 12)
				printf("Reserved for video-related blocks (%02x)\n", x[1]);
			else if (x[1] >= 19 && x[1] <= 31)
				printf("Reserved for audio-related blocks (%02x)\n", x[1]);
			else
				printf("Reserved (%02x)\n", x[1]);
			hex_block("  ", x + 2, length - 1);
			break;
		}
		break;
	default: {
		unsigned tag = (*x & 0xe0) >> 5;
		unsigned length = *x & 0x1f;
		printf("  Unknown tag %u, length %u (raw %02x)\n", tag, length, *x);
		break;
	}
	}
	first_block = 0;
	last_block_was_hdmi_vsdb = 0;
}

static int parse_cta(const unsigned char *x)
{
	int ret = 0; // 0 = conformant
	unsigned version = x[1];
	unsigned offset = x[2];
	const unsigned char *detailed;

	cur_block = "CTA-861";

	if (has_serial_number && has_serial_string)
		fail("Both the serial number and the serial string are set\n");

	if (version >= 1) do {
		if (version == 1 && x[3] != 0)
			ret = 1; // 1 = nonconformant

		if (offset < 4)
			break;

		if (version < 3) {
			printf("%u 8-byte timing descriptors\n\n", (offset - 4) / 8);
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
			printf("%u native detailed modes\n\n", x[3] & 0x0f);
		}
		if (version == 3) {
			int i;

			printf("%u bytes of CTA data\n", offset - 4);
			for (i = 4; i < offset; i += (x[i] & 0x1f) + 1) {
				cta_block(x + i);
			}
			printf("\n");
		}

		cur_block = "CTA-861 Detailed Timings";
		for (detailed = x + offset; detailed + 18 < x + 127; detailed += 18)
			if (detailed[0])
				detailed_block(detailed, 1);
	} while (0);

	has_valid_cta_checksum = do_checksum(x, EDID_PAGE_SIZE);
	has_cta861 = 1;
	nonconformant_cta861_640x480 = !has_cta861_vic_1 && !has_640x480p60_est_timing;

	return ret;
}

static void parse_displayid_detailed_timing(const unsigned char *x)
{
	unsigned ha, hbl, hso, hspw;
	unsigned va, vbl, vso, vspw;
	char phsync, pvsync, *stereo;
	unsigned pix_clock;
	char *aspect;

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
	printf("    Aspect %s%s%s\n", aspect, x[3] & 0x80 ? ", preferred" : "", stereo);
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
	
	printf("    Detailed mode: Clock %.3f MHz, %u mm x %u mm\n"
	       "                   %4u %4u %4u %4u (%3u %3u %3d)\n"
	       "                   %4u %4u %4u %4u (%3u %3u %3d)\n"
	       "                   %chsync %cvsync\n"
	       "                   VertFreq: %.3f Hz, HorFreq: %.3f kHz\n",
	       (float)pix_clock/100.0, 0, 0,
	       ha, ha + hso, ha + hso + hspw, ha + hbl, hso, hspw, hbl - hso - hspw,
	       va, va + vso, va + vso + vspw, va + vbl, vso, vspw, vbl - vso - vspw,
	       phsync, pvsync,
	       (pix_clock * 10000.0) / ((ha + hbl) * (va + vbl)),
	       (pix_clock * 10.0) / (ha + hbl)
	      );
}

static int parse_displayid(const unsigned char *x)
{
	const unsigned char *orig = x;
	unsigned version = x[1];
	unsigned length = x[2];
	unsigned ext_count = x[4];
	unsigned i;

	cur_block = "DisplayID";

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
					print_short_timings("    ", find_dmt_id(i + 1), "");
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

	/* DisplayID length field is number of following bytes
	 * but checksum is calculated over the entire structure
	 * (excluding DisplayID-in-EDID magic byte)
	 */
	printf("  ");
	has_valid_displayid_checksum = do_checksum(orig+1, orig[2] + 5);
	return 0;
}

/* generic extension code */

static void extension_version(const unsigned char *x)
{
	printf("Extension version: %u\n", x[1]);
}

static int parse_extension(const unsigned char *x)
{
	int nonconformant_extension = 0;

	printf("\n");

	switch (x[0]) {
	case 0x02:
		printf("CTA Extension Block\n");
		extension_version(x);
		nonconformant_extension = parse_cta(x);
		break;
	case 0x10:
		printf("VTB Extension Block\n");
		extension_version(x);
		hex_block("  ", x + 2, 125);
		do_checksum(x, EDID_PAGE_SIZE);
		break;
	case 0x40:
		printf("DI Extension Block\n");
		extension_version(x);
		hex_block("  ", x + 2, 125);
		do_checksum(x, EDID_PAGE_SIZE);
		break;
	case 0x50:
		printf("LS Extension Block\n");
		extension_version(x);
		hex_block("  ", x + 2, 125);
		do_checksum(x, EDID_PAGE_SIZE);
		break;
	case 0x60:
		printf("DPVL Extension Block\n");
		extension_version(x);
		hex_block("  ", x + 2, 125);
		do_checksum(x, EDID_PAGE_SIZE);
		break;
	case 0x70:
		printf("DisplayID Extension Block\n");
		nonconformant_extension = parse_displayid(x);
		do_checksum(x, EDID_PAGE_SIZE);
		break;
	case 0xf0:
		printf("Block map\n");
		hex_block("  ", x + 1, 126);
		do_checksum(x, EDID_PAGE_SIZE);
		break;
	case 0xff:
		printf("Manufacturer-specific Extension Block\n");
		extension_version(x);
		hex_block("  ", x + 2, 125);
		do_checksum(x, EDID_PAGE_SIZE);
		break;
	default:
		printf("Unknown Extension Block (0x%02x)\n", x[0]);
		extension_version(x);
		hex_block("  ", x + 2, 125);
		printf("  ");
		do_checksum(x, EDID_PAGE_SIZE);
		break;
	}

	return nonconformant_extension;
}

static int edid_lines = 0;

static unsigned char *extract_edid(int fd)
{
	char *ret = NULL;
	char *start, *c;
	unsigned char *out = NULL;
	unsigned state = 0;
	unsigned lines = 0;
	int i;
	unsigned out_index = 0;
	unsigned len, size;

	size = 1 << 10;
	ret = malloc(size);
	len = 0;

	if (ret == NULL)
		return NULL;

	for (;;) {
		i = read(fd, ret + len, size - len);
		if (i < 0) {
			free(ret);
			return NULL;
		}
		if (i == 0)
			break;
		len += i;
		if (len == size) {
			char *t;
			size <<= 1;
			t = realloc(ret, size);
			if (t == NULL) {
				free(ret);
				return NULL;
			}
			ret = t;
		}
	}

	start = strstr(ret, "EDID_DATA:");
	if (start == NULL)
		start = strstr(ret, "EDID:");
	/* Look for xrandr --verbose output (lines of 16 hex bytes) */
	if (start != NULL) {
		const char indentation1[] = "                ";
		const char indentation2[] = "\t\t";
		/* Used to detect that we've gone past the EDID property */
		const char half_indentation1[] = "        ";
		const char half_indentation2[] = "\t";
		const char *indentation;
		char *s;

		lines = 0;
		for (i = 0;; i++) {
			unsigned j;

			/* Get the next start of the line of EDID hex, assuming spaces for indentation */
			s = strstr(start, indentation = indentation1);
			/* Did we skip the start of another property? */
			if (s && s > strstr(start, half_indentation1))
				break;

			/* If we failed, retry assuming tabs for indentation */
			if (!s) {
				s = strstr(start, indentation = indentation2);
				/* Did we skip the start of another property? */
				if (s && s > strstr(start, half_indentation2))
					break;
			}

			if (!s)
				break;

			lines++;
			start = s + strlen(indentation);

			s = realloc(out, lines * 16);
			if (!s) {
				free(ret);
				free(out);
				return NULL;
			}
			out = (unsigned char *)s;
			c = start;
			for (j = 0; j < 16; j++) {
				char buf[3];
				/* Read a %02x from the log */
				if (!isxdigit(c[0]) || !isxdigit(c[1])) {
					if (j != 0) {
						lines--;
						break;
					}
					free(ret);
					free(out);
					return NULL;
				}
				buf[0] = c[0];
				buf[1] = c[1];
				buf[2] = 0;
				out[out_index++] = strtol(buf, NULL, 16);
				c += 2;
			}
			cur_block = "CTA-861";
		}

		free(ret);
		edid_lines = lines;
		return out;
	}

	start = strstr(ret, "<BLOCK");
	if (start) {
		/* Parse QuantumData 980 EDID files */
		do {
			start = strstr(start, ">");
			if (start)
				out = realloc(out, out_index + 128);
			if (!start || !out) {
				free(ret);
				free(out);
				return NULL;
			}
			start++;
			for (i = 0; i < 256; i += 2) {
				char buf[3];

				buf[0] = start[i];
				buf[1] = start[i + 1];
				buf[2] = 0;
				out[out_index++] = strtol(buf, NULL, 16);
			}
			start = strstr(start, "<BLOCK");
		} while (start);
		edid_lines = out_index >> 4;
		return out;
	}

	/* Is the EDID provided in hex? */
	for (i = 0; i < 32 && (isspace(ret[i]) || ret[i] == ',' ||
			       tolower(ret[i]) == 'x' || isxdigit(ret[i])); i++);
	if (i == 32) {
		out = malloc(size >> 1);
		if (out == NULL) {
			free(ret);
			return NULL;
		}

		for (c=ret; *c; c++) {
			char buf[3];

			if (!isxdigit(*c) || (*c == '0' && tolower(c[1]) == 'x'))
				continue;

			/* Read a %02x from the log */
			if (!isxdigit(c[0]) || !isxdigit(c[1])) {
				free(ret);
				free(out);
				return NULL;
			}

			buf[0] = c[0];
			buf[1] = c[1];
			buf[2] = 0;

			out[out_index++] = strtol(buf, NULL, 16);
			c++;
		}

		free(ret);
		edid_lines = out_index >> 4;
		return out;
	}

	/* wait, is this a log file? */
	for (i = 0; i < 8; i++) {
		if (!isascii(ret[i])) {
			edid_lines = len / 16;
			return (unsigned char *)ret;
		}
	}

	/* I think it is, let's go scanning */
	if (!(start = strstr(ret, "EDID (in hex):")))
		return (unsigned char *)ret;
	if (!(start = strstr(start, "(II)")))
		return (unsigned char *)ret;

	for (c = start; *c; c++) {
		if (state == 0) {
			char *s;
			/* skip ahead to the : */
			s = strstr(c, ": \t");
			if (!s)
				s = strstr(c, ":     ");
			if (!s)
				break;
			c = s;
			/* and find the first number */
			while (!isxdigit(c[1]))
				c++;
			state = 1;
			lines++;
			s = realloc(out, lines * 16);
			if (!s) {
				free(ret);
				free(out);
				return NULL;
			}
			out = (unsigned char *)s;
		} else if (state == 1) {
			char buf[3];
			/* Read a %02x from the log */
			if (!isxdigit(*c)) {
				state = 0;
				continue;
			}
			buf[0] = c[0];
			buf[1] = c[1];
			buf[2] = 0;
			out[out_index++] = strtol(buf, NULL, 16);
			c++;
		}
	}

	edid_lines = lines;

	free(ret);

	return out;
}

static void print_subsection(char *name, const unsigned char *edid,
			     unsigned start, unsigned end)
{
	unsigned i;

	printf("%s:", name);
	for (i = strlen(name); i < 15; i++)
		printf(" ");
	for (i = start; i <= end; i++)
		printf(" %02x", edid[i]);
	printf("\n");
}

static void dump_breakdown(const unsigned char *edid)
{
	printf("Extracted contents:\n");
	print_subsection("header", edid, 0, 7);
	print_subsection("serial number", edid, 8, 17);
	print_subsection("version", edid,18, 19);
	print_subsection("basic params", edid, 20, 24);
	print_subsection("chroma info", edid, 25, 34);
	print_subsection("established", edid, 35, 37);
	print_subsection("standard", edid, 38, 53);
	print_subsection("descriptor 1", edid, 54, 71);
	print_subsection("descriptor 2", edid, 72, 89);
	print_subsection("descriptor 3", edid, 90, 107);
	print_subsection("descriptor 4", edid, 108, 125);
	print_subsection("extensions", edid, 126, 126);
	print_subsection("checksum", edid, 127, 127);
	printf("\n");
}

static unsigned char crc_calc(const unsigned char *b)
{
	unsigned char sum = 0;
	unsigned i;

	for (i = 0; i < 127; i++)
		sum += b[i];
	return 256 - sum;
}

static int crc_ok(const unsigned char *b)
{
	return crc_calc(b) == b[127];
}

static void hexdumpedid(FILE *f, const unsigned char *edid, unsigned size)
{
	unsigned b, i, j;

	for (b = 0; b < size / 128; b++) {
		const unsigned char *buf = edid + 128 * b;

		if (b)
			fprintf(f, "\n");
		for (i = 0; i < 128; i += 0x10) {
			fprintf(f, "%02x", buf[i]);
			for (j = 1; j < 0x10; j++) {
				fprintf(f, " %02x", buf[i + j]);
			}
			fprintf(f, "\n");
		}
		if (!crc_ok(buf))
			fprintf(f, "Block %u has a checksum error (should be 0x%02x)\n",
				b, crc_calc(buf));
	}
}

static void carraydumpedid(FILE *f, const unsigned char *edid, unsigned size)
{
	unsigned b, i, j;

	fprintf(f, "unsigned char edid[] = {\n");
	for (b = 0; b < size / 128; b++) {
		const unsigned char *buf = edid + 128 * b;

		if (b)
			fprintf(f, "\n");
		for (i = 0; i < 128; i += 8) {
			fprintf(f, "\t0x%02x,", buf[i]);
			for (j = 1; j < 8; j++) {
				fprintf(f, " 0x%02x,", buf[i + j]);
			}
			fprintf(f, "\n");
		}
		if (!crc_ok(buf))
			fprintf(f, "\t/* Block %u has a checksum error (should be 0x%02x) */\n",
				b, crc_calc(buf));
	}
	fprintf(f, "};\n");
}

static void write_edid(FILE *f, const unsigned char *edid, unsigned size,
		       enum output_format out_fmt)
{
	switch (out_fmt) {
	default:
	case OUT_FMT_HEX:
		hexdumpedid(f, edid, size);
		break;
	case OUT_FMT_RAW:
		fwrite(edid, size, 1, f);
		break;
	case OUT_FMT_CARRAY:
		carraydumpedid(f, edid, size);
		break;
	}
}

static int edid_from_file(const char *from_file, const char *to_file,
			  enum output_format out_fmt)
{
	int fd;
	FILE *out = NULL;
	unsigned char *edid;
	unsigned char *x;
	time_t the_time;
	struct tm *ptm;
	int analog, i;
	unsigned col_x, col_y;

	if (!from_file || !strcmp(from_file, "-")) {
		fd = 0;
	} else if ((fd = open(from_file, O_RDONLY)) == -1) {
		perror(from_file);
		return -1;
	}
	if (to_file) {
		if (!strcmp(to_file, "-")) {
			out = stdout;
		} else if ((out = fopen(to_file, "w")) == NULL) {
			perror(to_file);
			return -1;
		}
		if (out_fmt == OUT_FMT_DEFAULT)
			out_fmt = out == stdout ? OUT_FMT_HEX : OUT_FMT_RAW;
	}

	edid = extract_edid(fd);
	if (!edid) {
		fprintf(stderr, "edid extract failed\n");
		return -1;
	}
	if (fd != 0)
		close(fd);

	if (out) {
		write_edid(out, edid, edid_lines * 16, out_fmt);
		if (out == stdout)
			return 0;
		fclose(out);
	}

	if (options[OptExtract])
		dump_breakdown(edid);

	if (!edid || memcmp(edid, "\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00", 8)) {
		fprintf(stderr, "No header found\n");
		return -1;
	}

	cur_block = "EDID Structure Version & Revision";
	printf("EDID version: %hhu.%hhu\n", edid[0x12], edid[0x13]);
	if (edid[0x12] == 1) {
		if (edid[0x13] > 4) {
			printf("Claims > 1.4, assuming 1.4 conformance\n");
			edid[0x13] = 4;
		}
		edid_minor = edid[0x13];
		switch (edid[0x13]) {
		case 4:
			claims_one_point_four = 1;
		case 3:
			claims_one_point_three = 1;
		case 2:
			claims_one_point_two = 1;
		default:
			break;
		}
		claims_one_point_oh = 1;
	}

	cur_block = "Vendor & Product Identification";
	printf("Manufacturer: %s Model %x Serial Number %u\n",
	       manufacturer_name(edid + 0x08),
	       (unsigned short)(edid[0x0a] + (edid[0x0b] << 8)),
	       (unsigned)(edid[0x0c] + (edid[0x0d] << 8) +
			  (edid[0x0e] << 16) + (edid[0x0f] << 24)));
	has_serial_number = edid[0x0c] || edid[0x0d] || edid[0x0e] || edid[0x0f];
	/* XXX need manufacturer ID table */

	time(&the_time);
	ptm = localtime(&the_time);
	if (edid[0x10] < 55 || (edid[0x10] == 0xff && claims_one_point_four)) {
		if (edid[0x11] <= 0x0f) {
			fail("bad year of manufacture\n");
		} else if (edid[0x10] == 0xff) {
			printf("Model year %u\n", edid[0x11] + 1990);
		} else if (edid[0x11] + 90 <= ptm->tm_year + 1) {
			if (edid[0x10])
				printf("Made in week %hhu of %u\n", edid[0x10], edid[0x11] + 1990);
			else
				printf("Made in year %u\n", edid[0x11] + 1990);
		} else {
			fail("bad year of manufacture\n");
		}
	} else {
		fail("bad week of manufacture\n");
	}

	/* display section */

	cur_block = "Basic Display Parameters & Features";
	if (edid[0x14] & 0x80) {
		int conformance_mask;
		analog = 0;
		printf("Digital display\n");
		if (claims_one_point_four) {
			conformance_mask = 0;
			if ((edid[0x14] & 0x70) == 0x00)
				printf("Color depth is undefined\n");
			else if ((edid[0x14] & 0x70) == 0x70)
				nonconformant_digital_display = 1;
			else
				printf("%u bits per primary color channel\n",
				       ((edid[0x14] & 0x70) >> 3) + 4);

			switch (edid[0x14] & 0x0f) {
			case 0x00: printf("Digital interface is not defined\n"); break;
			case 0x01: printf("DVI interface\n"); break;
			case 0x02: printf("HDMI-a interface\n"); break;
			case 0x03: printf("HDMI-b interface\n"); break;
			case 0x04: printf("MDDI interface\n"); break;
			case 0x05: printf("DisplayPort interface\n"); break;
			default:
				   nonconformant_digital_display = 1;
			}
		} else if (claims_one_point_two) {
			conformance_mask = 0x7e;
			if (edid[0x14] & 0x01) {
				printf("DFP 1.x compatible TMDS\n");
			}
		} else conformance_mask = 0x7f;
		if (!nonconformant_digital_display)
			nonconformant_digital_display = edid[0x14] & conformance_mask;
	} else {
		unsigned voltage = (edid[0x14] & 0x60) >> 5;
		unsigned sync = (edid[0x14] & 0x0f);

		analog = 1;
		printf("Analog display, Input voltage level: %s V\n",
		       voltage == 3 ? "0.7/0.7" :
		       voltage == 2 ? "1.0/0.4" :
		       voltage == 1 ? "0.714/0.286" :
		       "0.7/0.3");

		if (claims_one_point_four) {
			if (edid[0x14] & 0x10)
				printf("Blank-to-black setup/pedestal\n");
			else
				printf("Blank level equals black level\n");
		} else if (edid[0x14] & 0x10) {
			/*
			 * XXX this is just the X text.  1.3 says "if set, display expects
			 * a blank-to-black setup or pedestal per appropriate Signal
			 * Level Standard".  Whatever _that_ means.
			 */
			printf("Configurable signal levels\n");
		}

		printf("Sync: %s%s%s%s\n", sync & 0x08 ? "Separate " : "",
		       sync & 0x04 ? "Composite " : "",
		       sync & 0x02 ? "SyncOnGreen " : "",
		       sync & 0x01 ? "Serration " : "");
	}

	if (edid[0x15] && edid[0x16]) {
		printf("Maximum image size: %u cm x %u cm\n", edid[0x15], edid[0x16]);
		max_display_width_mm = edid[0x15] * 10;
		max_display_height_mm = edid[0x16] * 10;
		if ((max_display_height_mm && !max_display_width_mm) ||
		    (max_display_width_mm && !max_display_height_mm))
			fail("invalid maximum image size\n");
		else if (max_display_width_mm < 100 || max_display_height_mm < 100)
			warn("dubious maximum image size (smaller than 10x10 cm)\n");
	}
	else if (claims_one_point_four && (edid[0x15] || edid[0x16])) {
		if (edid[0x15])
			printf("Aspect ratio is %f (landscape)\n", 100.0/(edid[0x16] + 99));
		else
			printf("Aspect ratio is %f (portrait)\n", 100.0/(edid[0x15] + 99));
	} else {
		/* Either or both can be zero for 1.3 and before */
		printf("Image size is variable\n");
	}

	if (edid[0x17] == 0xff) {
		if (claims_one_point_four)
			printf("Gamma is defined in an extension block\n");
		else
			/* XXX Technically 1.3 doesn't say this... */
			printf("Gamma: 1.0\n");
	} else printf("Gamma: %.2f\n", ((edid[0x17] + 100.0) / 100.0));

	if (edid[0x18] & 0xe0) {
		printf("DPMS levels:");
		if (edid[0x18] & 0x80) printf(" Standby");
		if (edid[0x18] & 0x40) printf(" Suspend");
		if (edid[0x18] & 0x20) printf(" Off");
		printf("\n");
	}

	if (analog || !claims_one_point_four) {
		switch (edid[0x18] & 0x18) {
		case 0x00: printf("Monochrome or grayscale display\n"); break;
		case 0x08: printf("RGB color display\n"); break;
		case 0x10: printf("Non-RGB color display\n"); break;
		case 0x18: printf("Undefined display color type\n");
		}
	} else {
		printf("Supported color formats: RGB 4:4:4");
		if (edid[0x18] & 0x08)
			printf(", YCrCb 4:4:4");
		if (edid[0x18] & 0x10)
			printf(", YCrCb 4:2:2");
		printf("\n");
	}

	if (edid[0x18] & 0x04) {
		/*
		 * The sRGB chromaticities are (x, y):
		 * red:   0.640,  0.330
		 * green: 0.300,  0.600
		 * blue:  0.150,  0.060
		 * white: 0.3127, 0.3290
		 */
		static const unsigned char srgb_chromaticity[10] = {
			0xee, 0x91, 0xa3, 0x54, 0x4c, 0x99, 0x26, 0x0f, 0x50, 0x54
		};
		printf("Default (sRGB) color space is primary color space\n");
		if (memcmp(edid + 0x19, srgb_chromaticity, sizeof(srgb_chromaticity)))
			fail("sRGB is signaled, but the chromaticities do not match\n");
	}
	if (edid[0x18] & 0x02) {
		if (claims_one_point_four)
			printf("First detailed timing includes the native pixel format and preferred refresh rate\n");
		else
			printf("First detailed timing is preferred timing\n");
		has_preferred_timing = 1;
	} else if (claims_one_point_four) {
		/* 1.4 always has a preferred timing and this bit means something else. */
		has_preferred_timing = 1;
	}

	if (edid[0x18] & 0x01) {
		if (claims_one_point_four)
			printf("Display is continuous frequency\n");
		else
			printf("Supports GTF timings within operating range\n");
	}

	cur_block = "Color Characteristics";
	printf("%s\n", cur_block);
	col_x = (edid[0x1b] << 2) | (edid[0x19] >> 6);
	col_y = (edid[0x1c] << 2) | ((edid[0x19] >> 4) & 3);
	printf("  Red:   0.%04u, 0.%04u\n",
	       (col_x * 10000) / 1024, (col_y * 10000) / 1024);
	col_x = (edid[0x1d] << 2) | ((edid[0x19] >> 2) & 3);
	col_y = (edid[0x1e] << 2) | (edid[0x19] & 3);
	printf("  Green: 0.%04u, 0.%04u\n",
	       (col_x * 10000) / 1024, (col_y * 10000) / 1024);
	col_x = (edid[0x1f] << 2) | (edid[0x1a] >> 6);
	col_y = (edid[0x20] << 2) | ((edid[0x1a] >> 4) & 3);
	printf("  Blue:  0.%04u, 0.%04u\n",
	       (col_x * 10000) / 1024, (col_y * 10000) / 1024);
	col_x = (edid[0x21] << 2) | ((edid[0x1a] >> 2) & 3);
	col_y = (edid[0x22] << 2) | (edid[0x1a] & 3);
	printf("  White: 0.%04u, 0.%04u\n",
	       (col_x * 10000) / 1024, (col_y * 10000) / 1024);

	cur_block = "Established Timings I & II";
	printf("%s\n", cur_block);
	for (i = 0; i < 17; i++)
		if (edid[0x23 + i / 8] & (1 << (7 - i % 8)))
			print_short_timings("  ", &established_timings12[i], "");
	has_640x480p60_est_timing = edid[0x23] & 0x20;

	cur_block = "Standard Timings";
	printf("%s\n", cur_block);
	for (i = 0; i < 8; i++)
		print_standard_timing(edid[0x26 + i * 2], edid[0x26 + i * 2 + 1]);

	/* detailed timings */
	has_valid_detailed_blocks = detailed_block(edid + 0x36, 0);
	if (has_preferred_timing && !did_detailed_timing)
		has_preferred_timing = 0; /* not really accurate... */
	has_valid_detailed_blocks &= detailed_block(edid + 0x48, 0);
	has_valid_detailed_blocks &= detailed_block(edid + 0x5a, 0);
	has_valid_detailed_blocks &= detailed_block(edid + 0x6c, 0);

	if (edid[0x7e])
		printf("Has %u extension block%s\n", edid[0x7e], edid[0x7e] > 1 ? "s" : "");

	has_valid_checksum = do_checksum(edid, EDID_PAGE_SIZE);

	x = edid;
	for (edid_lines /= 8; edid_lines > 1; edid_lines--) {
		x += EDID_PAGE_SIZE;
		printf("\n----------------\n");
		nonconformant_extension += parse_extension(x);
	}

	if (!options[OptCheck]) {
		free(edid);
		return 0;
	}

	printf("\n----------------\n\n");

	if (claims_one_point_three) {
		if (nonconformant_digital_display ||
		    nonconformant_hf_vsdb_position ||
		    duplicate_scdb ||
		    nonconformant_hdmi_vsdb_tmds_rate ||
		    nonconformant_hf_vsdb_tmds_rate ||
		    nonconformant_hf_eeodb ||
		    nonconformant_cta861_640x480 ||
		    !has_name_descriptor ||
		    !has_preferred_timing ||
		    (!claims_one_point_four && !has_range_descriptor))
			conformant = 0;
		if (!conformant)
			printf("EDID block does NOT conform to EDID 1.%u!\n", edid_minor);
		if (nonconformant_digital_display)
			printf("\tDigital display field contains garbage: 0x%x\n",
			       nonconformant_digital_display);
		if (nonconformant_cta861_640x480)
			printf("\tRequired 640x480p60 timings are missing in the established timings\n"
			       "\tand/or in the SVD list (VIC 1)\n");
		if (nonconformant_hf_vsdb_position)
			printf("\tHDMI Forum VSDB or SCDB did not immediately follow the HDMI VSDB\n");
		if (duplicate_scdb)
			printf("\tDuplicate HDMI Forum VSDB/SCDB\n");
		if (nonconformant_hdmi_vsdb_tmds_rate)
			printf("\tHDMI VSDB Max TMDS rate is > 340\n");
		if (nonconformant_hf_vsdb_tmds_rate)
			printf("\tHDMI Forum VSDB Max TMDS rate is > 0 and <= 340 or > 600\n");
		if (nonconformant_hf_eeodb)
			printf("\tHDMI Forum EDID Extension Override Data Block starts at a wrong offset or is too long or reports a 0 block count\n");
		if (!has_name_descriptor)
			printf("\tMissing name descriptor\n");
		if (!has_preferred_timing)
			printf("\tMissing preferred timing\n");
		if (!has_range_descriptor)
			printf("\tMissing monitor ranges\n");
	} else if (claims_one_point_two) {
		if (nonconformant_digital_display)
			conformant = 0;
		if (!conformant)
			printf("EDID block does NOT conform to EDID 1.2!\n");
		if (nonconformant_digital_display)
			printf("\tDigital display field contains garbage: %x\n",
			       nonconformant_digital_display);
	} else if (claims_one_point_oh) {
		if (seen_non_detailed_descriptor)
			conformant = 0;
		if (!conformant)
			printf("EDID block does NOT conform to EDID 1.0!\n");
		if (seen_non_detailed_descriptor)
			printf("\tHas descriptor blocks other than detailed timings\n");
	}

	if (has_range_descriptor &&
	    (min_vert_freq_hz < mon_min_vert_freq_hz ||
	     max_vert_freq_hz > mon_max_vert_freq_hz ||
	     min_hor_freq_hz < mon_min_hor_freq_hz ||
	     max_hor_freq_hz > mon_max_hor_freq_hz ||
	     max_pixclk_khz > mon_max_pixclk_khz)) {
		/*
		 * EDID 1.4 states (in an Errata) that explicitly defined
		 * timings supersede the monitor range definition.
		 */
		if (!claims_one_point_four)
			conformant = 0;
		else
			printf("Warning: ");
		printf("One or more of the timings is out of range of the Monitor Ranges:\n");
		printf("  Vertical Freq: %u - %u Hz (Monitor: %u - %u Hz)\n",
		       min_vert_freq_hz, max_vert_freq_hz,
		       mon_min_vert_freq_hz, mon_max_vert_freq_hz);
		printf("  Horizontal Freq: %u - %u Hz (Monitor: %u - %u Hz)\n",
		       min_hor_freq_hz, max_hor_freq_hz,
		       mon_min_hor_freq_hz, mon_max_hor_freq_hz);
		printf("  Maximum Clock: %.3f MHz (Monitor: %.3f MHz)\n",
		       max_pixclk_khz / 1000.0, mon_max_pixclk_khz / 1000.0);
	}

	if (nonconformant_extension ||
	    !has_valid_checksum ||
	    !has_valid_detailed_blocks ||
	    !has_valid_descriptor_ordering ||
	    (has_name_descriptor && !has_valid_name_descriptor)) {
		conformant = 0;
		printf("EDID block does not conform:\n");
		if (nonconformant_extension)
			printf("\tHas %d nonconformant extension block(s)\n",
			       nonconformant_extension);
		if (!has_valid_checksum)
			printf("\tBlock has broken checksum\n");
		if (!has_valid_detailed_blocks)
			printf("\tDetailed blocks filled with garbage\n");
		if (!has_valid_descriptor_ordering)
			printf("\tInvalid detailed timing descriptor ordering\n");
		if (has_name_descriptor && !has_valid_name_descriptor)
			printf("\tInvalid Monitor Name descriptor\n");
	}

	if (!has_valid_cta_checksum) {
		printf("CTA extension block does not conform\n");
		printf("\tBlock has broken checksum\n");
	}
	if (!has_valid_displayid_checksum) {
		printf("DisplayID extension block does not conform\n");
		printf("\tBlock has broken checksum\n");
	}

	if ((supported_hdmi_vic_vsb_codes & supported_hdmi_vic_codes) != supported_hdmi_vic_codes)
		printf("Warning: HDMI VIC Codes must have their CTA-861 VIC equivalents in the VSB\n");

	free(edid);
	if (s_fail)
		printf("\nFailures:\n\n%s", s_fail);
	if (s_warn)
		printf("\nWarnings:\n\n%s", s_warn);
	printf("\nEDID conformity: %s\n", conformant ? "PASS" : "FAIL");
	return conformant ? 0 : -2;
}

int main(int argc, char **argv)
{
	char short_options[26 * 2 * 2 + 1];
	enum output_format out_fmt = OUT_FMT_DEFAULT;
	int ch;
	unsigned i;

	while (1) {
		int option_index = 0;
		unsigned idx = 0;

		for (i = 0; long_options[i].name; i++) {
			if (!isalpha(long_options[i].val))
				continue;
			short_options[idx++] = long_options[i].val;
			if (long_options[i].has_arg == required_argument)
				short_options[idx++] = ':';
		}
		short_options[idx] = 0;
		ch = getopt_long(argc, argv, short_options,
				 long_options, &option_index);
		if (ch == -1)
			break;

		options[ch] = 1;
		switch (ch) {
		case OptHelp:
			usage();
			return -1;
		case OptOutputFormat:
			if (!strcmp(optarg, "hex")) {
				out_fmt = OUT_FMT_HEX;
			} else if (!strcmp(optarg, "raw")) {
				out_fmt = OUT_FMT_RAW;
			} else if (!strcmp(optarg, "carray")) {
				out_fmt = OUT_FMT_CARRAY;
			} else {
				usage();
				exit(1);
			}
			break;
		case ':':
			fprintf(stderr, "Option `%s' requires a value\n",
				argv[optind]);
			usage();
			return -1;
		case '?':
			fprintf(stderr, "Unknown argument `%s'\n",
				argv[optind]);
			usage();
			return -1;
		}
	}
	if (optind == argc)
		return edid_from_file(NULL, NULL, out_fmt);
	if (optind == argc - 1)
		return edid_from_file(argv[optind], NULL, out_fmt);
	return edid_from_file(argv[optind], argv[optind + 1], out_fmt);
}

/*
 * Notes on panel extensions: (TODO, implement me in the code)
 *
 * EPI: http://www.epi-standard.org/fileadmin/spec/EPI_Specification1.0.pdf
 * at offset 0x6c (fourth detailed block): (all other bits reserved)
 * 0x6c: 00 00 00 0e 00
 * 0x71: bit 6-5: data color mapping (00 conventional/fpdi/vesa, 01 openldi)
 *       bit 4-3: pixels per clock (00 1, 01 2, 10 4, 11 reserved)
 *       bit 2-0: bits per pixel (000 18, 001 24, 010 30, else reserved)
 * 0x72: bit 5: FPSCLK polarity (0 normal 1 inverted)
 *       bit 4: DE polarity (0 high active 1 low active)
 *       bit 3-0: interface (0000 LVDS TFT
 *                           0001 mono STN 4/8bit
 *                           0010 color STN 8/16 bit
 *                           0011 18 bit tft
 *                           0100 24 bit tft
 *                           0101 tmds
 *                           else reserved)
 * 0x73: bit 1: horizontal display mode (0 normal 1 right/left reverse)
 *       bit 0: vertical display mode (0 normal 1 up/down reverse)
 * 0x74: bit 7-4: total poweroff seq delay (0000 vga controller default
 *                                          else time in 10ms (10ms to 150ms))
 *       bit 3-0: total poweron seq delay (as above)
 * 0x75: contrast power on/off seq delay, same as 0x74
 * 0x76: bit 7: backlight control enable (1 means this field is valid)
 *       bit 6: backlight enabled at boot (0 on 1 off)
 *       bit 5-0: backlight brightness control steps (0..63)
 * 0x77: bit 7: contrast control, same bit pattern as 0x76 except bit 6 resvd
 * 0x78 - 0x7c: reserved
 * 0x7d: bit 7-4: EPI descriptor major version (1)
 *       bit 3-0: EPI descriptor minor version (0)
 *
 * ----
 *
 * SPWG: http://www.spwg.org/spwg_spec_version3.8_3-14-2007.pdf
 *
 * Since these are "dummy" blocks, terminate with 0a 20 20 20 ... as usual
 *
 * detailed descriptor 3:
 * 0x5a - 0x5e: 00 00 00 fe 00
 * 0x5f - 0x63: PC maker part number
 * 0x64: LCD supplier revision #
 * 0x65 - 0x6b: manufacturer part number
 *
 * detailed descriptor 4:
 * 0x6c - 0x70: 00 00 00 fe 00
 * 0x71 - 0x78: smbus nits values (whut)
 * 0x79: number of lvds channels (1 or 2)
 * 0x7a: panel self test (1 if present)
 * and then dummy terminator
 *
 * SPWG also says something strange about the LSB of detailed descriptor 1:
 * "LSB is set to "1" if panel is DE-timing only. H/V can be ignored."
 */
