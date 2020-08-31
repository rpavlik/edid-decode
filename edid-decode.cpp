// SPDX-License-Identifier: MIT
/*
 * Copyright 2006-2012 Red Hat, Inc.
 * Copyright 2018-2020 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 *
 * Author: Adam Jackson <ajax@nwnk.net>
 * Maintainer: Hans Verkuil <hverkuil-cisco@xs4all.nl>
 */

#include <ctype.h>
#include <fcntl.h>
#include <getopt.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "edid-decode.h"

static edid_state state;

static unsigned char edid[EDID_PAGE_SIZE * EDID_MAX_BLOCKS];
static bool odd_hex_digits;

enum output_format {
	OUT_FMT_DEFAULT,
	OUT_FMT_HEX,
	OUT_FMT_RAW,
	OUT_FMT_CARRAY
};

/*
 * Options
 * Please keep in alphabetical order of the short option.
 * That makes it easier to see which options are still free.
 */
enum Option {
	OptCheck = 'c',
	OptCheckInline = 'C',
	OptExtract = 'e',
	OptHelp = 'h',
	OptNativeTimings = 'n',
	OptOutputFormat = 'o',
	OptPreferredTimings = 'p',
	OptPhysicalAddress = 'P',
	OptLongTimings = 'L',
	OptShortTimings = 'S',
	OptFBModeTimings = 'F',
	OptXModeLineTimings = 'X',
	OptV4L2Timings = 'V',
	OptSkipHexDump = 's',
	OptSkipSHA = 128,
	OptLast = 256
};

static char options[OptLast];

static struct option long_options[] = {
	{ "help", no_argument, 0, OptHelp },
	{ "output-format", required_argument, 0, OptOutputFormat },
	{ "extract", no_argument, 0, OptExtract },
	{ "native-timings", no_argument, 0, OptNativeTimings },
	{ "preferred-timings", no_argument, 0, OptPreferredTimings },
	{ "physical-address", no_argument, 0, OptPhysicalAddress },
	{ "skip-hex-dump", no_argument, 0, OptSkipHexDump },
	{ "skip-sha", no_argument, 0, OptSkipSHA },
	{ "check-inline", no_argument, 0, OptCheckInline },
	{ "check", no_argument, 0, OptCheck },
	{ "short-timings", no_argument, 0, OptShortTimings },
	{ "long-timings", no_argument, 0, OptLongTimings },
	{ "xmodeline", no_argument, 0, OptXModeLineTimings },
	{ "fbmode", no_argument, 0, OptFBModeTimings },
	{ "v4l2-timings", no_argument, 0, OptV4L2Timings },
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
	       "  -c, --check           check if the EDID conforms to the standards, failures and\n"
	       "                        warnings are reported at the end.\n"
	       "  -C, --check-inline    check if the EDID conforms to the standards, failures and\n"
	       "                        warnings are reported inline.\n"
	       "  -n, --native-timings  report the native timings\n"
	       "  -p, --preferred-timings report the preferred timings\n"
	       "  -P, --physical-address only report the CEC physical address\n"
	       "  -S, --short-timings   report all video timings in a short format\n"
	       "  -L, --long-timings    report all video timings in a long format\n"
	       "  -X, --xmodeline       report all long video timings in Xorg.conf format\n"
	       "  -F, --fbmode          report all long video timings in fb.modes format\n"
	       "  -V, --v4l2-timings    report all long video timings in v4l2-dv-timings.h format\n"
	       "  -s, --skip-hex-dump   skip the initial hex dump of the EDID\n"
	       "  --skip-sha            skip the SHA report\n"
	       "  -e, --extract         extract the contents of the first block in hex values\n"
	       "  -h, --help            display this help message\n");
}

static std::string s_msgs[EDID_MAX_BLOCKS + 1][2];

void msg(bool is_warn, const char *fmt, ...)
{
	char buf[1024] = "";
	va_list ap;

	va_start(ap, fmt);
	vsprintf(buf, fmt, ap);
	va_end(ap);

	if (is_warn)
		state.warnings++;
	else
		state.failures++;
	if (state.data_block.empty())
		s_msgs[state.block_nr][is_warn] += std::string("  ") + buf;
	else
		s_msgs[state.block_nr][is_warn] += "  " + state.data_block + ": " + buf;

	if (options[OptCheckInline])
		printf("%s: %s", is_warn ? "WARN" : "FAIL", buf);
}

static void show_msgs(bool is_warn)
{
	printf("\n%s:\n\n", is_warn ? "Warnings" : "Failures");
	for (unsigned i = 0; i < state.num_blocks; i++) {
		if (s_msgs[i][is_warn].empty())
			continue;
		printf("Block %u, %s:\n%s",
		       i, block_name(edid[i * EDID_PAGE_SIZE]).c_str(),
		       s_msgs[i][is_warn].c_str());
	}
	if (s_msgs[EDID_MAX_BLOCKS][is_warn].empty())
		return;
	printf("EDID:\n%s",
	       s_msgs[EDID_MAX_BLOCKS][is_warn].c_str());
}


void do_checksum(const char *prefix, const unsigned char *x, size_t len)
{
	unsigned char check = x[len - 1];
	unsigned char sum = 0;
	unsigned i;

	printf("%sChecksum: 0x%02hhx", prefix, check);

	for (i = 0; i < len-1; i++)
		sum += x[i];

	if ((unsigned char)(check + sum) != 0) {
		printf(" (should be 0x%02x)\n", -sum & 0xff);
		fail("Invalid checksum 0x%02x (should be 0x%02x).\n",
		     check, -sum & 0xff);
		return;
	}
	printf("\n");
}

static unsigned gcd(unsigned a, unsigned b)
{
	while (b) {
		unsigned t = b;

		b = a % b;
		a = t;
	}
	return a;
}

void calc_ratio(struct timings *t)
{
	unsigned d = gcd(t->hact, t->vact);

	if (d == 0) {
		t->hratio = t->vratio = 0;
		return;
	}
	t->hratio = t->hact / d;
	t->vratio = t->vact / d;
}

std::string edid_state::dtd_type(unsigned cnt)
{
	unsigned len = std::to_string(cta.preparse_total_dtds).length();
	char buf[16];
	sprintf(buf, "DTD %*u", len, cnt);
	return buf;
}

static void or_str(std::string &s, const std::string &flag, unsigned &num_flags)
{
	if (!num_flags)
		s = flag;
	else if (num_flags % 2 == 0)
		s = s + " | \\\n\t\t" + flag;
	else
		s = s + " | " + flag;
	num_flags++;
}

static void print_modeline(unsigned indent, const struct timings *t, double refresh)
{
	unsigned offset = (!t->even_vtotal && t->interlaced) ? 1 : 0;

	printf("%*sModeline \"%ux%u_%.2f%s\" %.3f  %u %u %u %u  %u %u %u %u  %cHSync",
	       indent, "",
	       t->hact, t->vact, refresh,
	       t->interlaced ? "i" : "", t->pixclk_khz / 1000.0,
	       t->hact, t->hact + t->hfp, t->hact + t->hfp + t->hsync,
	       t->hact + t->hfp + t->hsync + t->hbp,
	       t->vact, t->vact + t->vfp, t->vact + t->vfp + t->vsync,
	       t->vact + t->vfp + t->vsync + t->vbp + offset,
	       t->pos_pol_hsync ? '+' : '-');
	if (!t->no_pol_vsync)
		printf(" %cVSync", t->pos_pol_vsync ? '+' : '-');
	if (t->interlaced)
		printf(" Interlace");
	printf("\n");
}

static void print_fbmode(unsigned indent, const struct timings *t,
			 double refresh, double hor_freq_khz)
{
	printf("%*smode \"%ux%u-%u%s\"\n",
	       indent, "",
	       t->hact, t->vact,
	       (unsigned)(0.5 + (t->interlaced ? refresh / 2.0 : refresh)),
	       t->interlaced ? "-lace" : "");
	printf("%*s# D: %.2f MHz, H: %.3f kHz, V: %.2f Hz\n",
	       indent + 8, "",
	       t->pixclk_khz / 1000.0, hor_freq_khz, refresh);
	printf("%*sgeometry %u %u %u %u 32\n",
	       indent + 8, "",
	       t->hact, t->vact, t->hact, t->vact);
	unsigned mult = t->interlaced ? 2 : 1;
	unsigned offset = !t->even_vtotal && t->interlaced;
	printf("%*stimings %llu %d %d %d %u %u %u\n",
	       indent + 8, "",
	       (unsigned long long)(1000000000.0 / (double)(t->pixclk_khz) + 0.5),
	       t->hbp, t->hfp, mult * t->vbp, mult * t->vfp + offset, t->hsync, mult * t->vsync);
	if (t->interlaced)
		printf("%*slaced true\n", indent + 8, "");
	if (t->pos_pol_hsync)
		printf("%*shsync high\n", indent + 8, "");
	if (t->pos_pol_vsync)
		printf("%*svsync high\n", indent + 8, "");
	printf("%*sendmode\n", indent, "");
}

static void print_v4l2_timing(const struct timings *t,
			      double refresh, const char *type)
{
	printf("\t#define V4L2_DV_BT_%uX%u%c%u_%02u { \\\n",
	       t->hact, t->vact, t->interlaced ? 'I' : 'P',
	       (unsigned)refresh, (unsigned)(0.5 + 100.0 * (refresh - (unsigned)refresh)));
	printf("\t\t.type = V4L2_DV_BT_656_1120, \\\n");
	printf("\t\tV4L2_INIT_BT_TIMINGS(%u, %u, %u, ",
	       t->hact, t->vact, t->interlaced);
	if (!t->pos_pol_hsync && !t->pos_pol_vsync)
		printf("0, \\\n");
	else if (t->pos_pol_hsync && t->pos_pol_vsync)
		printf("\\\n\t\t\tV4L2_DV_HSYNC_POS_POL | V4L2_DV_VSYNC_POS_POL, \\\n");
	else if (t->pos_pol_hsync)
		printf("V4L2_DV_HSYNC_POS_POL, \\\n");
	else
		printf("V4L2_DV_VSYNC_POS_POL, \\\n");
	printf("\t\t\t%lluULL, %d, %u, %d, %u, %u, %d, %u, %u, %d, \\\n",
	       t->pixclk_khz * 1000ULL, t->hfp, t->hsync, t->hbp,
	       t->vfp, t->vsync, t->vbp,
	       t->interlaced ? t->vfp : 0,
	       t->interlaced ? t->vsync : 0,
	       t->interlaced ? t->vbp + !t->even_vtotal : 0);

	std::string flags;
	unsigned num_flags = 0;
	unsigned vic = 0;
	unsigned hdmi_vic = 0;
	const char *std = "0";

	if (t->interlaced && !t->even_vtotal)
		or_str(flags, "V4L2_DV_FL_HALF_LINE", num_flags);
	if (!memcmp(type, "VIC", 3)) {
		or_str(flags, "V4L2_DV_FL_HAS_CEA861_VIC", num_flags);
		or_str(flags, "V4L2_DV_FL_IS_CE_VIDEO", num_flags);
		vic = strtoul(type + 4, 0, 0);
	}
	if (!memcmp(type, "HDMI VIC", 8)) {
		or_str(flags, "V4L2_DV_FL_HAS_HDMI_VIC", num_flags);
		or_str(flags, "V4L2_DV_FL_IS_CE_VIDEO", num_flags);
		hdmi_vic = strtoul(type + 9, 0, 0);
		vic = hdmi_vic_to_vic(hdmi_vic);
		if (vic)
			or_str(flags, "V4L2_DV_FL_HAS_CEA861_VIC", num_flags);
	}
	if (vic && (fmod(refresh, 6)) == 0.0)
		or_str(flags, "V4L2_DV_FL_CAN_REDUCE_FPS", num_flags);
	if (t->rb)
		or_str(flags, "V4L2_DV_FL_REDUCED_BLANKING", num_flags);
	if (t->hratio && t->vratio)
		or_str(flags, "V4L2_DV_FL_HAS_PICTURE_ASPECT", num_flags);

	if (!memcmp(type, "VIC", 3) || !memcmp(type, "HDMI VIC", 8))
		std = "V4L2_DV_BT_STD_CEA861";
	else if (!memcmp(type, "DMT", 3))
		std = "V4L2_DV_BT_STD_DMT";
	else if (!memcmp(type, "CVT", 3))
		std = "V4L2_DV_BT_STD_CVT";
	else if (!memcmp(type, "GTF", 3))
		std = "V4L2_DV_BT_STD_GTF";
	printf("\t\t\t%s, \\\n", std);
	printf("\t\t\t%s, \\\n", flags.empty() ? "0" : flags.c_str());
	printf("\t\t\t{ %u, %u }, %u, %u) \\\n",
	       t->hratio, t->vratio, vic, hdmi_vic);
	printf("\t}\n");
}

static void print_detailed_timing(unsigned indent, const struct timings *t)
{
	printf("%*sHfront %4d Hsync %3u Hback %3d Hpol %s",
	       indent, "",
	       t->hfp, t->hsync, t->hbp, t->pos_pol_hsync ? "P" : "N");
	if (t->hborder)
		printf(" Hborder %u", t->hborder);
	printf("\n");

	printf("%*sVfront %4u Vsync %3u Vback %3d",
	       indent, "", t->vfp, t->vsync, t->vbp);
	if (!t->no_pol_vsync)
		printf(" Vpol %s", t->pos_pol_vsync ? "P" : "N");
	if (t->vborder)
		printf(" Vborder %u", t->vborder);
	if (t->even_vtotal) {
		printf(" Both Fields");
	} else if (t->interlaced) {
		printf(" Vfront +0.5 Odd Field\n");
		printf("%*sVfront %4d Vsync %3u Vback %3d",
		       indent, "", t->vfp, t->vsync, t->vbp);
		if (!t->no_pol_vsync)
			printf(" Vpol %s", t->pos_pol_vsync ? "P" : "N");
		if (t->vborder)
			printf(" Vborder %u", t->vborder);
		printf(" Vback  +0.5 Even Field");
	}
	printf("\n");
}

bool edid_state::print_timings(const char *prefix, const struct timings *t,
			       const char *type, const char *flags,
			       bool detailed)
{
	if (!t) {
		// Should not happen
		fail("Unknown video timings.\n");
		return false;
	}

	if (detailed && options[OptShortTimings])
		detailed = false;
	if (options[OptLongTimings])
		detailed = true;

	unsigned vact = t->vact;
	unsigned hbl = t->hfp + t->hsync + t->hbp;
	unsigned vbl = t->vfp + t->vsync + t->vbp;
	unsigned htotal = t->hact + hbl;
	double hor_freq_khz = htotal ? (double)t->pixclk_khz / htotal : 0;

	if (t->interlaced)
		vact /= 2;

	if (t->ycbcr420)
		hor_freq_khz /= 2;

	double vtotal = vact + vbl;

	bool ok = true;

	if (!t->hact || !hbl || !t->hfp || !t->hsync ||
	    !vact || !vbl || (!t->vfp && !t->interlaced && !t->even_vtotal) || !t->vsync) {
		fail("0 values in the video timing:\n"
		     "    Horizontal Active/Blanking %u/%u\n"
		     "    Horizontal Frontporch/Sync Width %u/%u\n"
		     "    Vertical Active/Blanking %u/%u\n"
		     "    Vertical Frontporch/Sync Width %u/%u\n",
		     t->hact, hbl, t->hfp, t->hsync, vact, vbl, t->vfp, t->vsync);
		ok = false;
	}

	if (t->even_vtotal)
		vtotal = vact + t->vfp + t->vsync + t->vbp;
	else if (t->interlaced)
		vtotal = vact + t->vfp + t->vsync + t->vbp + 0.5;

	double refresh = (double)t->pixclk_khz * 1000.0 / (htotal * vtotal);

	std::string s;
	if (t->rb) {
		s = "RB";
		if (t->rb == 2)
			s += "v2";
	}
	add_str(s, flags);
	if (t->hsize_mm || t->vsize_mm)
		add_str(s, std::to_string(t->hsize_mm) + " mm x " + std::to_string(t->vsize_mm) + " mm");
	if (!s.empty())
		s = " (" + s + ")";
	unsigned pixclk_khz = t->pixclk_khz / (t->ycbcr420 ? 2 : 1);

	char buf[10];

	sprintf(buf, "%u%s", t->vact, t->interlaced ? "i" : "");
	printf("%s%s: %5ux%-5s %7.3f Hz %3u:%-3u %7.3f kHz %7.3f MHz%s\n",
	       prefix, type,
	       t->hact, buf,
	       refresh,
	       t->hratio, t->vratio,
	       hor_freq_khz,
	       pixclk_khz / 1000.0,
	       s.c_str());

	unsigned len = strlen(prefix) + 2;

	if (!t->ycbcr420 && detailed && options[OptXModeLineTimings])
		print_modeline(len, t, refresh);
	else if (!t->ycbcr420 && detailed && options[OptFBModeTimings])
		print_fbmode(len, t, refresh, hor_freq_khz);
	else if (!t->ycbcr420 && detailed && options[OptV4L2Timings])
		print_v4l2_timing(t, refresh, type);
	else if (detailed)
		print_detailed_timing(len + strlen(type) + 6, t);

	if (t->ycbcr420 && t->pixclk_khz < 590000)
		warn_once("Some YCbCr 4:2:0 timings are invalid for HDMI (which requires an RGB timings pixel rate >= 590 MHz).\n");
	if (t->hfp <= 0)
		fail("0 or negative horizontal front porch.\n");
	if (t->hbp <= 0)
		fail("0 or negative horizontal back porch.\n");
	if (t->vbp <= 0)
		fail("0 or negative vertical back porch.\n");
	if ((!base.max_display_width_mm && t->hsize_mm) ||
	    (!base.max_display_height_mm && t->vsize_mm)) {
		fail("Mismatch of image size vs display size: image size is set, but not display size.\n");
	} else if (!t->hsize_mm && !t->vsize_mm) {
		/* this is valid */
	} else if (t->hsize_mm > base.max_display_width_mm + 9 ||
		   t->vsize_mm > base.max_display_height_mm + 9) {
		fail("Mismatch of image size %ux%u mm vs display size %ux%u mm.\n",
		     t->hsize_mm, t->vsize_mm, base.max_display_width_mm, base.max_display_height_mm);
	} else if (t->hsize_mm < base.max_display_width_mm - 9 &&
		   t->vsize_mm < base.max_display_height_mm - 9) {
		fail("Mismatch of image size %ux%u mm vs display size %ux%u mm.\n",
		     t->hsize_mm, t->vsize_mm, base.max_display_width_mm, base.max_display_height_mm);
	}
	if (refresh) {
		min_vert_freq_hz = min(min_vert_freq_hz, refresh);
		max_vert_freq_hz = max(max_vert_freq_hz, refresh);
	}
	if (pixclk_khz && (t->hact + hbl)) {
		min_hor_freq_hz = min(min_hor_freq_hz, (pixclk_khz * 1000) / (t->hact + hbl));
		max_hor_freq_hz = max(max_hor_freq_hz, (pixclk_khz * 1000) / (t->hact + hbl));
		max_pixclk_khz = max(max_pixclk_khz, pixclk_khz);
	}
	return ok;
}

std::string utohex(unsigned char x)
{
	char buf[10];

	sprintf(buf, "0x%02hhx", x);
	return buf;
}

const char *oui_name(unsigned oui, bool reverse)
{
	if (reverse)
		oui = (oui >> 16) | (oui & 0xff00) | ((oui & 0xff) << 16);

	switch (oui) {
	case 0x00001a: return "AMD";
	case 0x000c03: return "HDMI";
	case 0x00044b: return "NVIDIA";
	case 0x000c6e: return "ASUS";
	case 0x0010fa: return "Apple";
	case 0x0014b9: return "MSTAR";
	case 0x00d046: return "Dolby";
	case 0x00e047: return "InFocus";
	case 0x3a0292: return "VESA";
	case 0x90848b: return "HDR10+";
	case 0xc45dd8: return "HDMI Forum";
	case 0xca125c: return "Microsoft";
	default: return NULL;
	}
}

std::string ouitohex(unsigned oui)
{
	char buf[32];

	sprintf(buf, "%02X-%02X-%02X", (oui >> 16) & 0xff, (oui >> 8) & 0xff, oui & 0xff);
	return buf;
}

bool memchk(const unsigned char *x, unsigned len, unsigned char v)
{
	for (unsigned i = 0; i < len; i++)
		if (x[i] != v)
			return false;
	return true;
}

void hex_block(const char *prefix, const unsigned char *x,
	       unsigned length, bool show_ascii, unsigned step)
{
	unsigned i, j;

	if (!length)
		return;

	for (i = 0; i < length; i += step) {
		unsigned len = min(step, length - i);

		printf("%s", prefix);
		for (j = 0; j < len; j++)
			printf("%s%02x", j ? " " : "", x[i + j]);

		if (show_ascii) {
			for (j = len; j < step; j++)
				printf("   ");
			printf(" '");
			for (j = 0; j < len; j++)
				printf("%c", x[i + j] >= ' ' && x[i + j] <= '~' ? x[i + j] : '.');
			printf("'");
		}
		printf("\n");
	}
}

static bool edid_add_byte(const char *s, bool two_digits = true)
{
	char buf[3];

	if (state.edid_size == sizeof(edid))
		return false;
	buf[0] = s[0];
	buf[1] = two_digits ? s[1] : 0;
	buf[2] = 0;
	edid[state.edid_size++] = strtoul(buf, NULL, 16);
	return true;
}

static bool extract_edid_quantumdata(const char *start)
{
	/* Parse QuantumData 980 EDID files */
	do {
		start = strstr(start, ">");
		if (!start)
			return false;
		start++;
		for (unsigned i = 0; start[i] && start[i + 1] && i < 256; i += 2)
			if (!edid_add_byte(start + i))
				return false;
		start = strstr(start, "<BLOCK");
	} while (start);
	return state.edid_size;
}

static const char *ignore_chars = ",:;";

static bool extract_edid_hex(const char *s, bool require_two_digits = true)
{
	for (; *s; s++) {
		if (isspace(*s) || strchr(ignore_chars, *s))
			continue;

		if (*s == '0' && tolower(s[1]) == 'x') {
			s++;
			continue;
		}

		/* Read one or two hex digits from the log */
		if (!isxdigit(s[0])) {
			if (state.edid_size && state.edid_size % 128 == 0)
				break;
			return false;
		}
		if (require_two_digits && !isxdigit(s[1])) {
			odd_hex_digits = true;
			return false;
		}
		if (!edid_add_byte(s, isxdigit(s[1])))
			return false;
		if (isxdigit(s[1]))
			s++;
	}
	return state.edid_size;
}

static bool extract_edid_xrandr(const char *start)
{
	static const char indentation1[] = "                ";
	static const char indentation2[] = "\t\t";
	/* Used to detect that we've gone past the EDID property */
	static const char half_indentation1[] = "        ";
	static const char half_indentation2[] = "\t";
	const char *indentation;
	const char *s;

	for (;;) {
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

		start = s + strlen(indentation);

		for (j = 0; j < 16; j++, start += 2) {
			/* Read a %02x from the log */
			if (!isxdigit(start[0]) || !isxdigit(start[1])) {
				if (j)
					break;
				return false;
			}
			if (!edid_add_byte(start))
				return false;
		}
	}
	return state.edid_size;
}

static bool extract_edid_xorg(const char *start)
{
	bool find_first_num = true;

	for (; *start; start++) {
		if (find_first_num) {
			const char *s;

			/* skip ahead to the : */
			s = strstr(start, ": \t");
			if (!s)
				s = strstr(start, ":     ");
			if (!s)
				break;
			start = s;
			/* and find the first number */
			while (!isxdigit(start[1]))
				start++;
			find_first_num = false;
			continue;
		} else {
			/* Read a %02x from the log */
			if (!isxdigit(*start)) {
				find_first_num = true;
				continue;
			}
			if (!edid_add_byte(start))
				return false;
			start++;
		}
	}
	return state.edid_size;
}

static bool extract_edid(int fd, FILE *error)
{
	std::vector<char> edid_data;
	char buf[EDID_PAGE_SIZE];

	for (;;) {
		ssize_t i = read(fd, buf, sizeof(buf));

		if (i < 0)
			return false;
		if (i == 0)
			break;
		edid_data.insert(edid_data.end(), buf, buf + i);
	}

	if (edid_data.empty())
		return false;

	const char *data = &edid_data[0];
	const char *start;

	/* Look for edid-decode output */
	start = strstr(data, "EDID (hex):");
	if (!start)
		start = strstr(data, "edid-decode (hex):");
	if (start)
		return extract_edid_hex(strchr(start, ':'));

	/* Look for C-array */
	start = strstr(data, "unsigned char edid[] = {");
	if (start)
		return extract_edid_hex(strchr(start, '{') + 1, false);

	/* Look for QuantumData EDID output */
	start = strstr(data, "<BLOCK");
	if (start)
		return extract_edid_quantumdata(start);

	/* Look for xrandr --verbose output (lines of 16 hex bytes) */
	start = strstr(data, "EDID_DATA:");
	if (!start)
		start = strstr(data, "EDID:");
	if (start)
		return extract_edid_xrandr(start);

	/* Look for an EDID in an Xorg.0.log file */
	start = strstr(data, "EDID (in hex):");
	if (start)
		start = strstr(start, "(II)");
	if (start)
		return extract_edid_xorg(start);

	unsigned i;

	/* Is the EDID provided in hex? */
	for (i = 0; i < 32 && (isspace(data[i]) || strchr(ignore_chars, data[i]) ||
			       tolower(data[i]) == 'x' || isxdigit(data[i])); i++);

	if (i == 32)
		return extract_edid_hex(data);

	/* Assume binary */
	if (edid_data.size() > sizeof(edid)) {
		fprintf(error, "Binary EDID length %zu is greater than %zu\n",
			edid_data.size(), sizeof(edid));
		return false;
	}
	memcpy(edid, data, edid_data.size());
	state.edid_size = edid_data.size();
	return true;
}

static void print_subsection(const char *name, const unsigned char *edid,
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

	fprintf(f, "const unsigned char edid[] = {\n");
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

static int edid_to_file(const char *to_file, enum output_format out_fmt)
{
	FILE *out;

	if (!strcmp(to_file, "-")) {
		to_file = "stdout";
		out = stdout;
	} else if ((out = fopen(to_file, "w")) == NULL) {
		perror(to_file);
		return -1;
	}
	if (out_fmt == OUT_FMT_DEFAULT)
		out_fmt = out == stdout ? OUT_FMT_HEX : OUT_FMT_RAW;

	switch (out_fmt) {
	default:
	case OUT_FMT_HEX:
		hexdumpedid(out, edid, state.edid_size);
		break;
	case OUT_FMT_RAW:
		fwrite(edid, state.edid_size, 1, out);
		break;
	case OUT_FMT_CARRAY:
		carraydumpedid(out, edid, state.edid_size);
		break;
	}

	if (out != stdout)
		fclose(out);
	return 0;
}

static int edid_from_file(const char *from_file, FILE *error)
{
#ifdef O_BINARY
	// Windows compatibility
	int flags = O_RDONLY | O_BINARY;
#else
	int flags = O_RDONLY;
#endif
	int fd;

	if (!strcmp(from_file, "-")) {
		from_file = "stdin";
		fd = 0;
	} else if ((fd = open(from_file, flags)) == -1) {
		perror(from_file);
		return -1;
	}

	odd_hex_digits = false;
	if (!extract_edid(fd, error)) {
		fprintf(error, "EDID extract of '%s' failed ", from_file);
		if (odd_hex_digits)
			fprintf(error, "(odd number of hexadecimal digits)\n");
		else
			fprintf(error, "(unknown format)\n");
		return -1;
	}
	if (state.edid_size % EDID_PAGE_SIZE) {
		fprintf(error, "EDID length %u is not a multiple of %u\n",
			state.edid_size, EDID_PAGE_SIZE);
		return -1;
	}
	state.num_blocks = state.edid_size / EDID_PAGE_SIZE;
	if (fd != 0)
		close(fd);

	if (memcmp(edid, "\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00", 8)) {
		fprintf(error, "No EDID header found in '%s'\n", from_file);
		return -1;
	}
	return 0;
}

/* generic extension code */

std::string block_name(unsigned char block)
{
	char buf[10];

	switch (block) {
	case 0x00: return "Base EDID";
	case 0x02: return "CTA-861 Extension Block";
	case 0x10: return "Video Timing Extension Block";
	case 0x20: return "EDID 2.0 Extension Block";
	case 0x40: return "Display Information Extension Block";
	case 0x50: return "Localized String Extension Block";
	case 0x60: return "Microdisplay Interface Extension Block";
	case 0x70: return "DisplayID Extension Block";
	case 0xf0: return "Block Map Extension Block";
	case 0xff: return "Manufacturer-Specific Extension Block";
	default:
		sprintf(buf, " 0x%02x", block);
		return std::string("Unknown EDID Extension Block") + buf;
	}
}

void edid_state::parse_block_map(const unsigned char *x)
{
	unsigned last_valid_block_tag = 0;
	bool fail_once = false;
	unsigned offset = 1;
	unsigned i;

	printf("%s\n", block.c_str());
	if (block_nr == 1)
		block_map.saw_block_1 = true;
	else if (!block_map.saw_block_1)
		fail("No EDID Block Map Extension found in block 1.\n");

	if (block_nr > 1)
		offset = 128;

	for (i = 1; i < 127; i++) {
		unsigned block = offset + i;

		if (x[i]) {
			last_valid_block_tag++;
			if (i != last_valid_block_tag && !fail_once) {
				fail("Valid block tags are not consecutive.\n");
				fail_once = true;
			}
			printf("  Block %3u: %s\n", block, block_name(block).c_str());
			if (block >= num_blocks && !fail_once) {
				fail("Invalid block number %u.\n", block);
				fail_once = true;
			}
		}
	}
}

void edid_state::preparse_extension(const unsigned char *x)
{
	switch (x[0]) {
	case 0x02:
		has_cta = true;
		preparse_cta_block(x);
		break;
	case 0x70:
		has_dispid = true;
		preparse_displayid_block(x);
		break;
	}
}

void edid_state::parse_extension(const unsigned char *x)
{
	block = block_name(x[0]);
	data_block.clear();

	printf("\n");
	if (block_nr && x[0] == 0)
		block = "Unknown EDID Extension Block 0x00";
	printf("Block %u, %s:\n", block_nr, block.c_str());

	switch (x[0]) {
	case 0x02:
		parse_cta_block(x);
		break;
	case 0x10:
		parse_vtb_ext_block(x);
		break;
	case 0x20:
		fail("Deprecated extension block for EDID 2.0, do not use.\n");
		break;
	case 0x40:
		parse_di_ext_block(x);
		break;
	case 0x50:
		parse_ls_ext_block(x);
		break;
	case 0x70:
		parse_displayid_block(x);
		break;
	case 0xf0:
		parse_block_map(x);
		if (block_nr != 1 && block_nr != 128)
			fail("Must be used in block 1 and 128.\n");
		break;
	default:
		hex_block("  ", x, EDID_PAGE_SIZE);
		fail("Unknown Extension Block.\n");
		break;
	}

	data_block.clear();
	do_checksum("", x, EDID_PAGE_SIZE);
}

int edid_state::parse_edid()
{
	for (unsigned i = 1; i < num_blocks; i++)
		preparse_extension(edid + i * EDID_PAGE_SIZE);

	if (options[OptPhysicalAddress]) {
		printf("%x.%x.%x.%x\n",
		       (cta.preparsed_phys_addr >> 12) & 0xf,
		       (cta.preparsed_phys_addr >> 8) & 0xf,
		       (cta.preparsed_phys_addr >> 4) & 0xf,
		       cta.preparsed_phys_addr & 0xf);
		return 0;
	}

	if (!options[OptSkipHexDump]) {
		printf("edid-decode (hex):\n\n");
		for (unsigned i = 0; i < num_blocks; i++) {
			hex_block("", edid + i * EDID_PAGE_SIZE, EDID_PAGE_SIZE, false);
			printf("\n");
		}
		printf("----------------\n\n");
	}

	if (options[OptExtract])
		dump_breakdown(edid);

	block = block_name(0x00);
	printf("Block %u, %s:\n", block_nr, block.c_str());
	parse_base_block(edid);

	for (unsigned i = 1; i < num_blocks; i++) {
		block_nr++;
		printf("\n----------------\n");
		parse_extension(edid + i * EDID_PAGE_SIZE);
	}

	block = "";
	block_nr = EDID_MAX_BLOCKS;

	if (has_cta)
		cta_resolve_svrs();

	if (options[OptPreferredTimings] && base.preferred_timing.is_valid()) {
		printf("\n----------------\n");
		printf("\nPreferred Video Timing (Block 0):\n");
		print_timings("  ", base.preferred_timing, true);
	}

	if (options[OptNativeTimings] &&
	    base.preferred_timing.is_valid() && base.preferred_is_also_native) {
		printf("\n----------------\n");
		printf("\nNative Video Timing (Block 0):\n");
		print_timings("  ", base.preferred_timing, true);
	}

	if (options[OptPreferredTimings] && !cta.preferred_timings.empty()) {
		printf("\n----------------\n");
		printf("\nPreferred Video Timing%s (CTA-861):\n",
		       cta.preferred_timings.size() > 1 ? "s" : "");
		for (vec_timings_ext::iterator iter = cta.preferred_timings.begin();
		     iter != cta.preferred_timings.end(); ++iter)
			print_timings("  ", *iter, true);
	}

	if (options[OptNativeTimings] && !cta.native_timings.empty()) {
		printf("\n----------------\n");
		printf("\nNative Video Timing%s (CTA-861):\n",
		       cta.native_timings.size() > 1 ? "s" : "");
		for (vec_timings_ext::iterator iter = cta.native_timings.begin();
		     iter != cta.native_timings.end(); ++iter)
			print_timings("  ", *iter, true);
	}

	if (options[OptPreferredTimings] && !dispid.preferred_timings.empty()) {
		printf("\n----------------\n");
		printf("\nPreferred Video Timing%s (DisplayID):\n",
		       dispid.preferred_timings.size() > 1 ? "s" : "");
		for (vec_timings_ext::iterator iter = dispid.preferred_timings.begin();
		     iter != dispid.preferred_timings.end(); ++iter)
			print_timings("  ", *iter, true);
	}

	if (!options[OptCheck] && !options[OptCheckInline])
		return 0;

	check_base_block();
	if (has_cta)
		check_cta_blocks();
	if (has_dispid)
		check_displayid_blocks();

	printf("\n----------------\n");

	if (!options[OptSkipSHA]) {
#define STR(x) #x
#define STRING(x) STR(x)
		printf("\nedid-decode SHA: %s\n", STRING(SHA));
	}

	if (options[OptCheck]) {
		if (warnings)
			show_msgs(true);
		if (failures)
			show_msgs(false);
	}
	printf("\nEDID conformity: %s\n", failures ? "FAIL" : "PASS");
	return failures ? -2 : 0;
}

int main(int argc, char **argv)
{
	char short_options[26 * 2 * 2 + 1];
	enum output_format out_fmt = OUT_FMT_DEFAULT;
	int ret;

	while (1) {
		int option_index = 0;
		unsigned idx = 0;
		unsigned i;

		for (i = 0; long_options[i].name; i++) {
			if (!isalpha(long_options[i].val))
				continue;
			short_options[idx++] = long_options[i].val;
			if (long_options[i].has_arg == required_argument)
				short_options[idx++] = ':';
		}
		short_options[idx] = 0;
		int ch = getopt_long(argc, argv, short_options,
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
			fprintf(stderr, "Option '%s' requires a value\n",
				argv[optind]);
			usage();
			return -1;
		case '?':
			fprintf(stderr, "Unknown argument '%s'\n",
				argv[optind]);
			usage();
			return -1;
		}
	}
	if (optind == argc)
		ret = edid_from_file("-", stdout);
	else
		ret = edid_from_file(argv[optind], argv[optind + 1] ? stderr : stdout);

	if (ret && options[OptPhysicalAddress]) {
		printf("f.f.f.f\n");
		return 0;
	}
	if (optind < argc - 1)
		return ret ? ret : edid_to_file(argv[optind + 1], out_fmt);

	return ret ? ret : state.parse_edid();
}

#ifdef __EMSCRIPTEN__
/*
 * The surrounding JavaScript implementation will call this function
 * each time it wants to decode an EDID. So this should reset all the
 * state and start over.
 */
extern "C" int parse_edid(const char *input)
{
	for (unsigned i = 0; i < EDID_MAX_BLOCKS + 1; i++) {
		s_msgs[i][0].clear();
		s_msgs[i][1].clear();
	}
	options[OptCheck] = 1;
	state = edid_state();
	int ret = edid_from_file(input);
	return ret ? ret : state.parse_edid();
}
#endif
