// SPDX-License-Identifier: MIT
/*
 * Copyright 2006-2015 Red Hat, Inc.
 * Copyright 2018-2020 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 *
 * Author: Adam Jackson <ajax@nwnk.net> and contributors
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

#define STR(x) #x
#define STRING(x) STR(x)

static edid_state state;

static unsigned char edid[EDID_PAGE_SIZE * EDID_MAX_BLOCKS];
static bool odd_hex_digits;

enum output_format {
	OUT_FMT_DEFAULT,
	OUT_FMT_HEX,
	OUT_FMT_RAW,
	OUT_FMT_CARRAY,
	OUT_FMT_XML,
};

/*
 * Options
 * Please keep in alphabetical order of the short option.
 * That makes it easier to see which options are still free.
 */
enum Option {
	OptCheck = 'c',
	OptCheckInline = 'C',
	OptFBModeTimings = 'F',
	OptHelp = 'h',
	OptOnlyHexDump = 'H',
	OptLongTimings = 'L',
	OptNativeTimings = 'n',
	OptOutputFormat = 'o',
	OptPreferredTimings = 'p',
	OptPhysicalAddress = 'P',
	OptSkipHexDump = 's',
	OptShortTimings = 'S',
	OptV4L2Timings = 'V',
	OptXModeLineTimings = 'X',
	OptSkipSHA = 128,
	OptHideSerialNumbers,
	OptVersion,
	OptSTD,
	OptDMT,
	OptVIC,
	OptHDMIVIC,
	OptCVT,
	OptGTF,
	OptListEstTimings,
	OptListDMTs,
	OptListVICs,
	OptListHDMIVICs,
	OptLast = 256
};

static char options[OptLast];

static struct option long_options[] = {
	{ "help", no_argument, 0, OptHelp },
	{ "output-format", required_argument, 0, OptOutputFormat },
	{ "native-timings", no_argument, 0, OptNativeTimings },
	{ "preferred-timings", no_argument, 0, OptPreferredTimings },
	{ "physical-address", no_argument, 0, OptPhysicalAddress },
	{ "skip-hex-dump", no_argument, 0, OptSkipHexDump },
	{ "only-hex-dump", no_argument, 0, OptOnlyHexDump },
	{ "skip-sha", no_argument, 0, OptSkipSHA },
	{ "hide-serial-numbers", no_argument, 0, OptHideSerialNumbers },
	{ "version", no_argument, 0, OptVersion },
	{ "check-inline", no_argument, 0, OptCheckInline },
	{ "check", no_argument, 0, OptCheck },
	{ "short-timings", no_argument, 0, OptShortTimings },
	{ "long-timings", no_argument, 0, OptLongTimings },
	{ "xmodeline", no_argument, 0, OptXModeLineTimings },
	{ "fbmode", no_argument, 0, OptFBModeTimings },
	{ "v4l2-timings", no_argument, 0, OptV4L2Timings },
	{ "std", required_argument, 0, OptSTD },
	{ "dmt", required_argument, 0, OptDMT },
	{ "vic", required_argument, 0, OptVIC },
	{ "hdmi-vic", required_argument, 0, OptHDMIVIC },
	{ "cvt", required_argument, 0, OptCVT },
	{ "gtf", required_argument, 0, OptGTF },
	{ "list-established-timings", no_argument, 0, OptListEstTimings },
	{ "list-dmts", no_argument, 0, OptListDMTs },
	{ "list-vics", no_argument, 0, OptListVICs },
	{ "list-hdmi-vics", no_argument, 0, OptListHDMIVICs },
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
	       "                        If [out] is specified, then write the EDID in this format\n"
	       "                        <fmt> is one of:\n"
	       "                        hex:    hex numbers in ascii text (default for stdout)\n"
	       "                        raw:    binary data (default unless writing to stdout)\n"
	       "                        carray: c-program struct\n"
	       "                        xml:    XML data\n"
	       "  -c, --check           Check if the EDID conforms to the standards, failures and\n"
	       "                        warnings are reported at the end.\n"
	       "  -C, --check-inline    Check if the EDID conforms to the standards, failures and\n"
	       "                        warnings are reported inline.\n"
	       "  -n, --native-timings  Report the native timings.\n"
	       "  -p, --preferred-timings Report the preferred timings.\n"
	       "  -P, --physical-address Only report the CEC physical address.\n"
	       "  -S, --short-timings   Report all video timings in a short format.\n"
	       "  -L, --long-timings    Report all video timings in a long format.\n"
	       "  -X, --xmodeline       Report all long video timings in Xorg.conf format.\n"
	       "  -F, --fbmode          Report all long video timings in fb.modes format.\n"
	       "  -V, --v4l2-timings    Report all long video timings in v4l2-dv-timings.h format.\n"
	       "  -s, --skip-hex-dump   Skip the initial hex dump of the EDID.\n"
	       "  -H, --only-hex-dump   Only output the hex dump of the EDID.\n"
	       "  --skip-sha            Skip the SHA report.\n"
	       "  --hide-serial-numbers Replace serial numbers with '...'\n"
	       "  --version             show the edid-decode version (SHA)\n"
	       "  --std <byte1>,<byte2> Show the standard timing represented by these two bytes.\n"
	       "  --dmt <dmt>           Show the timings for the DMT with the given DMT ID.\n"
	       "  --vic <vic>           Show the timings for this VIC.\n"
	       "  --hdmi-vic <hdmivic>  Show the timings for this HDMI VIC.\n"
	       "  --cvt w=<width>,h=<height>,fps=<fps>[,rb=<rb>][,interlaced][,overscan][,alt]\n"
	       "                        Calculate the CVT timings for the given format.\n"
	       "                        <fps> is frames per second for progressive timings,\n"
	       "                        or fields per second for interlaced timings.\n"
	       "                        <rb> can be 0 (no reduced blanking, default), or\n"
	       "                        1-3 for the reduced blanking version.\n"
	       "                        If 'interlaced' is given, then this is an interlaced format.\n"
	       "                        If 'overscan' is given, then this is an overscanned format.\n"
	       "                        If 'alt' is given and <rb>=2, then report the timings\n"
	       "                        optimized for video: 1000 / 1001 * <fps>.\n"
	       "                        If 'alt' is given and <rb>=3, then the horizontal blanking\n"
	       "                        is 160 instead of 80 pixels.\n"
	       "  --gtf w=<width>,h=<height>[,fps=<fps>][,horfreq=<horfreq>][,pixclk=<pixclk>][,interlaced]\n"
	       "        [,overscan][,secondary][,C=<c>][,M=<m>][,K=<k>][,J=<j>]\n"
	       "                        Calculate the GTF timings for the given format.\n"
	       "                        <fps> is frames per second for progressive timings,\n"
	       "                        or fields per second for interlaced timings.\n"
	       "                        <horfreq> is the horizontal frequency in kHz.\n"
	       "                        <pixclk> is the pixel clock frequency in MHz.\n"
	       "                        Only one of fps, horfreq or pixclk must be given.\n"
	       "                        If 'interlaced' is given, then this is an interlaced format.\n"
	       "                        If 'overscan' is given, then this is an overscanned format.\n"
	       "                        If 'secondary' is given, then the secondary GTF is used for\n"
	       "                        reduced blanking, where <c>, <m>, <k> and <j> are parameters\n"
	       "                        for the secondary curve.\n"
	       "  --list-established-timings List all known Established Timings.\n"
	       "  --list-dmts           List all known DMTs.\n"
	       "  --list-vics           List all known VICs.\n"
	       "  --list-hdmi-vics      List all known HDMI VICs.\n"
	       "  -h, --help            Display this help message.\n");
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
	unsigned len = std::to_string(cta.preparsed_total_dtds).length();
	char buf[16];
	sprintf(buf, "DTD %*u", len, cnt);
	return buf;
}

bool edid_state::match_timings(const timings &t1, const timings &t2)
{
	if (t1.hact != t2.hact ||
	    t1.vact != t2.vact ||
	    t1.rb != t2.rb ||
	    t1.interlaced != t2.interlaced ||
	    t1.hfp != t2.hfp ||
	    t1.hbp != t2.hbp ||
	    t1.hsync != t2.hsync ||
	    t1.pos_pol_hsync != t2.pos_pol_hsync ||
	    t1.hratio != t2.hratio ||
	    t1.vfp != t2.vfp ||
	    t1.vbp != t2.vbp ||
	    t1.vsync != t2.vsync ||
	    t1.pos_pol_vsync != t2.pos_pol_vsync ||
	    t1.vratio != t2.vratio ||
	    t1.pixclk_khz != t2.pixclk_khz)
		return false;
	return true;
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

/*
 * Return true if the timings are a close, but not identical,
 * match. The only differences allowed are polarities and
 * porches and syncs, provided the total blanking remains the
 * same.
 */
bool timings_close_match(const timings &t1, const timings &t2)
{
	// We don't want to deal with borders, you're on your own
	// if you are using those.
	if (t1.hborder || t1.vborder ||
	    t2.hborder || t2.vborder)
		return false;
	if (t1.hact != t2.hact || t1.vact != t2.vact ||
	    t1.interlaced != t2.interlaced ||
	    t1.pixclk_khz != t2.pixclk_khz ||
	    t1.hfp + t1.hsync + t1.hbp != t2.hfp + t2.hsync + t2.hbp ||
	    t1.vfp + t1.vsync + t1.vbp != t2.vfp + t2.vsync + t2.vbp)
		return false;
	if (t1.hfp == t2.hfp &&
	    t1.hsync == t2.hsync &&
	    t1.hbp == t2.hbp &&
	    t1.pos_pol_hsync == t2.pos_pol_hsync &&
	    t1.vfp == t2.vfp &&
	    t1.vsync == t2.vsync &&
	    t1.vbp == t2.vbp &&
	    t1.pos_pol_vsync == t2.pos_pol_vsync)
		return false;
	return true;
}

static void print_modeline(unsigned indent, const struct timings *t, double refresh)
{
	unsigned offset = (!t->even_vtotal && t->interlaced) ? 1 : 0;
	unsigned hfp = t->hborder + t->hfp;
	unsigned hbp = t->hborder + t->hbp;
	unsigned vfp = t->vborder + t->vfp;
	unsigned vbp = t->vborder + t->vbp;

	printf("%*sModeline \"%ux%u_%.2f%s\" %.3f  %u %u %u %u  %u %u %u %u  %cHSync",
	       indent, "",
	       t->hact, t->vact, refresh,
	       t->interlaced ? "i" : "", t->pixclk_khz / 1000.0,
	       t->hact, t->hact + hfp, t->hact + hfp + t->hsync,
	       t->hact + hfp + t->hsync + hbp,
	       t->vact, t->vact + vfp, t->vact + vfp + t->vsync,
	       t->vact + vfp + t->vsync + vbp + offset,
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
	unsigned hfp = t->hborder + t->hfp;
	unsigned hbp = t->hborder + t->hbp;
	unsigned vfp = t->vborder + t->vfp;
	unsigned vbp = t->vborder + t->vbp;
	printf("%*stimings %llu %d %d %d %u %u %u\n",
	       indent + 8, "",
	       (unsigned long long)(1000000000.0 / (double)(t->pixclk_khz) + 0.5),
	       hbp, hfp, mult * vbp, mult * vfp + offset, t->hsync, mult * t->vsync);
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
	unsigned hfp = t->hborder + t->hfp;
	unsigned hbp = t->hborder + t->hbp;
	unsigned vfp = t->vborder + t->vfp;
	unsigned vbp = t->vborder + t->vbp;
	printf("\t\t\t%lluULL, %d, %u, %d, %u, %u, %d, %u, %u, %d, \\\n",
	       t->pixclk_khz * 1000ULL, hfp, t->hsync, hbp,
	       vfp, t->vsync, vbp,
	       t->interlaced ? vfp : 0,
	       t->interlaced ? t->vsync : 0,
	       t->interlaced ? vbp + !t->even_vtotal : 0);

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
			       bool detailed, bool do_checks)
{
	if (!t) {
		// Should not happen
		if (do_checks)
			fail("Unknown video timings.\n");
		return false;
	}

	if (detailed && options[OptShortTimings])
		detailed = false;
	if (options[OptLongTimings])
		detailed = true;

	unsigned vact = t->vact;
	unsigned hbl = t->hfp + t->hsync + t->hbp + 2 * t->hborder;
	unsigned vbl = t->vfp + t->vsync + t->vbp + 2 * t->vborder;
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
		if (do_checks)
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
	unsigned rb = t->rb & ~RB_ALT;
	if (rb) {
		bool alt = t->rb & RB_ALT;
		s = "RB";
		// Mark RB_CVT_V3 as preliminary since CVT 1.3 has not been
		// released yet.
		if (rb == RB_CVT_V2)
			s += std::string("v2") + (alt ? ",video-optimized" : "");
		else if (rb == RB_CVT_V3)
			s += std::string("v3-is-preliminary") + (alt ? ",h-blank-160" : "");
	}
	add_str(s, flags);
	if (t->hsize_mm || t->vsize_mm)
		add_str(s, std::to_string(t->hsize_mm) + " mm x " + std::to_string(t->vsize_mm) + " mm");
	if (t->hsize_mm > dtd_max_hsize_mm)
		dtd_max_hsize_mm = t->hsize_mm;
	if (t->vsize_mm > dtd_max_vsize_mm)
		dtd_max_vsize_mm = t->vsize_mm;
	if (!s.empty())
		s = " (" + s + ")";
	unsigned pixclk_khz = t->pixclk_khz / (t->ycbcr420 ? 2 : 1);

	char buf[10];

	sprintf(buf, "%u%s", t->vact, t->interlaced ? "i" : "");
	printf("%s%s: %5ux%-5s %7.3f Hz %3u:%-3u %7.3f kHz %8.3f MHz%s\n",
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

	if (!do_checks)
		return ok;

	if (!memcmp(type, "DTD", 3)) {
		unsigned vic, dmt;
		const timings *vic_t = cta_close_match_to_vic(*t, vic);

		if (vic_t)
			warn("DTD is similar but not identical to VIC %u.\n", vic);

		const timings *dmt_t = close_match_to_dmt(*t, dmt);
		if (!vic_t && dmt_t)
			warn("DTD is similar but not identical to DMT 0x%02x.\n", dmt);
	}

	if (refresh) {
		min_vert_freq_hz = min(min_vert_freq_hz, refresh);
		max_vert_freq_hz = max(max_vert_freq_hz, refresh);
	}
	if (hor_freq_khz) {
		min_hor_freq_hz = min(min_hor_freq_hz, hor_freq_khz * 1000.0);
		max_hor_freq_hz = max(max_hor_freq_hz, hor_freq_khz * 1000.0);
		max_pixclk_khz = max(max_pixclk_khz, pixclk_khz);
		if (t->pos_pol_hsync && !t->pos_pol_vsync && t->vsync == 3)
			base.max_pos_neg_hor_freq_khz = hor_freq_khz;
	}

	if (t->ycbcr420 && t->pixclk_khz < 590000)
		warn_once("Some YCbCr 4:2:0 timings are invalid for HDMI (which requires an RGB timings pixel rate >= 590 MHz).\n");
	if (t->hfp <= 0)
		fail("0 or negative horizontal front porch.\n");
	if (t->hbp <= 0)
		fail("0 or negative horizontal back porch.\n");
	if (t->vbp <= 0)
		fail("0 or negative vertical back porch.\n");
	if (!base.max_display_width_mm && !base.max_display_height_mm) {
		/* this is valid */
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
	return ok;
}

std::string containerid2s(const unsigned char *x)
{
	char buf[40];

	sprintf(buf, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
		x[0], x[1], x[2], x[3],
		x[4], x[5],
		x[6], x[7],
		x[8], x[9],
		x[10], x[11], x[12], x[13], x[14], x[15]);
	return buf;
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

	if (edid_data.empty()) {
		state.edid_size = 0;
		return false;
	}

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
		fprintf(error, "Binary EDID length %zu is greater than %zu.\n",
			edid_data.size(), sizeof(edid));
		return false;
	}
	memcpy(edid, data, edid_data.size());
	state.edid_size = edid_data.size();
	return true;
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
			fprintf(f, "Block %u has a checksum error (should be 0x%02x).\n",
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
			fprintf(f, "\t/* Block %u has a checksum error (should be 0x%02x). */\n",
				b, crc_calc(buf));
	}
	fprintf(f, "};\n");
}

// This format can be read by the QuantumData EDID editor
static void xmldumpedid(FILE *f, const unsigned char *edid, unsigned size)
{
	fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n");
	fprintf(f, "<DATAOBJ>\n");
	fprintf(f, "    <HEADER TYPE=\"DID\" VERSION=\"1.0\"/>\n");
	fprintf(f, "    <DATA>\n");
	for (unsigned b = 0; b < size / 128; b++) {
		const unsigned char *buf = edid + 128 * b;

		fprintf(f, "        <BLOCK%u>", b);
		for (unsigned i = 0; i < 128; i++)
			fprintf(f, "%02X", buf[i]);
		fprintf(f, "</BLOCK%u>\n", b);
	}
	fprintf(f, "    </DATA>\n");
	fprintf(f, "</DATAOBJ>\n");
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
	case OUT_FMT_XML:
		xmldumpedid(out, edid, state.edid_size);
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
		if (!state.edid_size) {
			fprintf(error, "EDID of '%s' was empty.\n", from_file);
			return -1;
		}
		fprintf(error, "EDID extract of '%s' failed: ", from_file);
		if (odd_hex_digits)
			fprintf(error, "odd number of hexadecimal digits.\n");
		else
			fprintf(error, "unknown format.\n");
		return -1;
	}
	if (state.edid_size % EDID_PAGE_SIZE) {
		fprintf(error, "EDID length %u is not a multiple of %u.\n",
			state.edid_size, EDID_PAGE_SIZE);
		return -1;
	}
	state.num_blocks = state.edid_size / EDID_PAGE_SIZE;
	if (fd != 0)
		close(fd);

	if (memcmp(edid, "\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00", 8)) {
		fprintf(error, "No EDID header found in '%s'.\n", from_file);
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

	if (block_nr == 1)
		block_map.saw_block_1 = true;
	else if (!block_map.saw_block_1)
		fail("No EDID Block Map Extension found in block 1.\n");
	else if (block_nr == 128)
		block_map.saw_block_128 = true;

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
			printf("  Block %3u: %s\n", block, block_name(x[i]).c_str());
			if (block >= num_blocks) {
				if (!fail_once)
					fail("Invalid block number %u.\n", block);
				fail_once = true;
			} else if (x[i] != edid[block * EDID_PAGE_SIZE]) {
				fail("Block %u tag mismatch: expected 0x%02x, but got 0x%02x.\n",
				     block, edid[block * EDID_PAGE_SIZE], x[i]);
			}
		} else if (block < num_blocks) {
			fail("Block %u tag mismatch: expected 0x%02x, but got 0x00.\n",
			     block, edid[block * EDID_PAGE_SIZE]);
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
	hide_serial_numbers = options[OptHideSerialNumbers];

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
			if (i == num_blocks - 1 && options[OptOnlyHexDump])
				return 0;
			printf("\n");
		}
		printf("----------------\n\n");
	}

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
		printf("\nPreferred Video Timing if only Block 0 is parsed:\n");
		print_timings("  ", base.preferred_timing, true, false);
	}

	if (options[OptNativeTimings] &&
	    base.preferred_timing.is_valid() && base.preferred_is_also_native) {
		printf("\n----------------\n");
		printf("\nNative Video Timing if only Block 0 is parsed:\n");
		print_timings("  ", base.preferred_timing, true, false);
	}

	if (options[OptPreferredTimings] && !cta.preferred_timings.empty()) {
		printf("\n----------------\n");
		printf("\nPreferred Video Timing%s if Block 0 and CTA-861 Blocks are parsed:\n",
		       cta.preferred_timings.size() > 1 ? "s" : "");
		for (vec_timings_ext::iterator iter = cta.preferred_timings.begin();
		     iter != cta.preferred_timings.end(); ++iter)
			print_timings("  ", *iter, true, false);
	}

	if (options[OptNativeTimings] && !cta.native_timings.empty()) {
		printf("\n----------------\n");
		printf("\nNative Video Timing%s if Block 0 and CTA-861 Blocks are parsed:\n",
		       cta.native_timings.size() > 1 ? "s" : "");
		for (vec_timings_ext::iterator iter = cta.native_timings.begin();
		     iter != cta.native_timings.end(); ++iter)
			print_timings("  ", *iter, true, false);
	}

	if (options[OptPreferredTimings] && !dispid.preferred_timings.empty()) {
		printf("\n----------------\n");
		printf("\nPreferred Video Timing%s if Block 0 and DisplayID Blocks are parsed:\n",
		       dispid.preferred_timings.size() > 1 ? "s" : "");
		for (vec_timings_ext::iterator iter = dispid.preferred_timings.begin();
		     iter != dispid.preferred_timings.end(); ++iter)
			print_timings("  ", *iter, true, false);
	}

	if (!options[OptCheck] && !options[OptCheckInline])
		return 0;

	check_base_block();
	if (has_cta)
		check_cta_blocks();
	if (has_dispid)
		check_displayid_blocks();

	printf("\n----------------\n");

	if (!options[OptSkipSHA] && strlen(STRING(SHA))) {
		printf("\nedid-decode SHA: %s %s\n", STRING(SHA), STRING(DATE));
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

enum cvt_opts {
	CVT_WIDTH = 0,
	CVT_HEIGHT,
	CVT_FPS,
	CVT_INTERLACED,
	CVT_OVERSCAN,
	CVT_RB,
	CVT_ALT,
};

static int parse_cvt_subopt(char **subopt_str, double *value)
{
	int opt;
	char *opt_str;

	static const char * const subopt_list[] = {
		"w",
		"h",
		"fps",
		"interlaced",
		"overscan",
		"rb",
		"alt",
		nullptr
	};

	opt = getsubopt(subopt_str, (char* const*) subopt_list, &opt_str);

	if (opt == -1) {
		fprintf(stderr, "Invalid suboptions specified.\n");
		usage();
		std::exit(EXIT_FAILURE);
	}
	if (opt_str == nullptr && opt != CVT_INTERLACED && opt != CVT_ALT &&
	    opt != CVT_OVERSCAN) {
		fprintf(stderr, "No value given to suboption <%s>.\n",
				subopt_list[opt]);
		usage();
		std::exit(EXIT_FAILURE);
	}

	if (opt_str)
		*value = strtod(opt_str, nullptr);
	return opt;
}

static void parse_cvt(char *optarg)
{
	unsigned w = 0, h = 0;
	double fps = 0;
	unsigned rb = RB_NONE;
	bool interlaced = false;
	bool alt = false;
	bool overscan = false;

	while (*optarg != '\0') {
		int opt;
		double opt_val;

		opt = parse_cvt_subopt(&optarg, &opt_val);

		switch (opt) {
		case CVT_WIDTH:
			w = round(opt_val);
			break;
		case CVT_HEIGHT:
			h = round(opt_val);
			break;
		case CVT_FPS:
			fps = opt_val;
			break;
		case CVT_RB:
			rb = opt_val;
			break;
		case CVT_OVERSCAN:
			overscan = true;
			break;
		case CVT_INTERLACED:
			interlaced = opt_val;
			break;
		case CVT_ALT:
			alt = opt_val;
			break;
		default:
			break;
		}
	}

	if (!w || !h || !fps) {
		fprintf(stderr, "Missing width, height and/or fps.\n");
		usage();
		std::exit(EXIT_FAILURE);
	}
	if (interlaced)
		fps /= 2;
	timings t = state.calc_cvt_mode(w, h, fps, rb, interlaced, overscan, alt);
	state.print_timings("", &t, "CVT", "", true, false);
}

struct gtf_parsed_data {
	unsigned w, h;
	double freq;
	double C, M, K, J;
	bool overscan;
	bool interlaced;
	bool secondary;
	bool params_from_edid;
	enum gtf_ip_parm ip_parm;
};

enum gtf_opts {
	GTF_WIDTH = 0,
	GTF_HEIGHT,
	GTF_FPS,
	GTF_HORFREQ,
	GTF_PIXCLK,
	GTF_INTERLACED,
	GTF_OVERSCAN,
	GTF_SECONDARY,
	GTF_C2,
	GTF_M,
	GTF_K,
	GTF_J2,
};

static int parse_gtf_subopt(char **subopt_str, double *value)
{
	int opt;
	char *opt_str;

	static const char * const subopt_list[] = {
		"w",
		"h",
		"fps",
		"horfreq",
		"pixclk",
		"interlaced",
		"overscan",
		"secondary",
		"C",
		"M",
		"K",
		"J",
		nullptr
	};

	opt = getsubopt(subopt_str, (char * const *)subopt_list, &opt_str);

	if (opt == -1) {
		fprintf(stderr, "Invalid suboptions specified.\n");
		usage();
		std::exit(EXIT_FAILURE);
	}
	if (opt_str == nullptr && opt != GTF_INTERLACED && opt != GTF_OVERSCAN &&
	    opt != GTF_SECONDARY) {
		fprintf(stderr, "No value given to suboption <%s>.\n",
				subopt_list[opt]);
		usage();
		std::exit(EXIT_FAILURE);
	}

	if (opt == GTF_C2 || opt == GTF_J2)
		*value = round(2.0 * strtod(opt_str, nullptr));
	else if (opt_str)
		*value = strtod(opt_str, nullptr);
	return opt;
}

static void parse_gtf(char *optarg, gtf_parsed_data &data)
{
	memset(&data, 0, sizeof(data));
	data.params_from_edid = true;
	data.C = 40;
	data.M = 600;
	data.K = 128;
	data.J = 20;

	while (*optarg != '\0') {
		int opt;
		double opt_val;

		opt = parse_gtf_subopt(&optarg, &opt_val);

		switch (opt) {
		case GTF_WIDTH:
			data.w = round(opt_val);
			break;
		case GTF_HEIGHT:
			data.h = round(opt_val);
			break;
		case GTF_FPS:
			data.freq = opt_val;
			data.ip_parm = gtf_ip_vert_freq;
			break;
		case GTF_HORFREQ:
			data.freq = opt_val;
			data.ip_parm = gtf_ip_hor_freq;
			break;
		case GTF_PIXCLK:
			data.freq = opt_val;
			data.ip_parm = gtf_ip_clk_freq;
			break;
		case GTF_INTERLACED:
			data.interlaced = true;
			break;
		case GTF_OVERSCAN:
			data.overscan = true;
			break;
		case GTF_SECONDARY:
			data.secondary = true;
			break;
		case GTF_C2:
			data.C = opt_val / 2.0;
			data.params_from_edid = false;
			break;
		case GTF_M:
			data.M = round(opt_val);
			data.params_from_edid = false;
			break;
		case GTF_K:
			data.K = round(opt_val);
			data.params_from_edid = false;
			break;
		case GTF_J2:
			data.J = opt_val / 2.0;
			data.params_from_edid = false;
			break;
		default:
			break;
		}
	}

	if (!data.w || !data.h) {
		fprintf(stderr, "Missing width and/or height.\n");
		usage();
		std::exit(EXIT_FAILURE);
	}
	if (!data.freq) {
		fprintf(stderr, "One of fps, horfreq or pixclk must be given.\n");
		usage();
		std::exit(EXIT_FAILURE);
	}
	if (!data.secondary)
		data.params_from_edid = false;
	if (data.interlaced && data.ip_parm == gtf_ip_vert_freq)
		data.freq /= 2;
}

static void show_gtf(gtf_parsed_data &data)
{
	timings t;

	t = state.calc_gtf_mode(data.w, data.h, data.freq, data.interlaced,
				data.ip_parm, data.overscan, data.secondary,
				data.C, data.M, data.K, data.J);
	calc_ratio(&t);
	state.print_timings("", &t, "GTF", "", true, false);
}

int main(int argc, char **argv)
{
	char short_options[26 * 2 * 2 + 1];
	enum output_format out_fmt = OUT_FMT_DEFAULT;
	gtf_parsed_data gtf_data;
	int ret;

	while (1) {
		int option_index = 0;
		unsigned idx = 0;
		unsigned i, val;
		const timings *t;
		char buf[16];

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
			} else if (!strcmp(optarg, "xml")) {
				out_fmt = OUT_FMT_XML;
			} else {
				usage();
				exit(1);
			}
			break;
		case OptSTD: {
			unsigned char byte1, byte2 = 0;
			char *endptr;

			byte1 = strtoul(optarg, &endptr, 0);
			if (*endptr == ',')
				byte2 = strtoul(endptr + 1, NULL, 0);
			state.print_standard_timing("", byte1, byte2, false, true);
			break;
		}
		case OptDMT:
			val = strtoul(optarg, NULL, 0);
			t = find_dmt_id(val);
			if (t) {
				sprintf(buf, "DMT 0x%02x", val);
				state.print_timings("", t, buf, "", true, false);
			} else {
				fprintf(stderr, "Unknown DMT code 0x%02x.\n", val);
			}
			break;
		case OptVIC:
			val = strtoul(optarg, NULL, 0);
			t = find_vic_id(val);
			if (t) {
				sprintf(buf, "VIC %3u", val);
				state.print_timings("", t, buf, "", true, false);
			} else {
				fprintf(stderr, "Unknown VIC code %u.\n", val);
			}
			break;
		case OptHDMIVIC:
			val = strtoul(optarg, NULL, 0);
			t = find_hdmi_vic_id(val);
			if (t) {
				sprintf(buf, "HDMI VIC %u", val);
				state.print_timings("", t, buf, "", true, false);
			} else {
				fprintf(stderr, "Unknown HDMI VIC code %u.\n", val);
			}
			break;
		case OptCVT:
			parse_cvt(optarg);
			break;
		case OptGTF:
			parse_gtf(optarg, gtf_data);
			break;
		case ':':
			fprintf(stderr, "Option '%s' requires a value.\n",
				argv[optind]);
			usage();
			return -1;
		case '?':
			fprintf(stderr, "Unknown argument '%s'.\n",
				argv[optind]);
			usage();
			return -1;
		}
	}
	if (optind == argc && options[OptVersion]) {
		if (strlen(STRING(SHA)))
			printf("edid-decode SHA: %s %s\n", STRING(SHA), STRING(DATE));
		else
			printf("edid-decode SHA: not available\n");
		return 0;
	}

	if (options[OptListEstTimings])
		state.list_established_timings();
	if (options[OptListDMTs])
		state.list_dmts();
	if (options[OptListVICs])
		state.cta_list_vics();
	if (options[OptListHDMIVICs])
		state.cta_list_hdmi_vics();

	if (options[OptListEstTimings] || options[OptListDMTs] ||
	    options[OptListVICs] || options[OptListHDMIVICs])
		return 0;

	if (options[OptCVT] || options[OptDMT] || options[OptVIC] ||
	    options[OptHDMIVIC] || options[OptSTD])
		return 0;

	if (options[OptGTF] && (!gtf_data.params_from_edid || optind == argc)) {
		show_gtf(gtf_data);
		return 0;
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

	if (options[OptGTF]) {
		timings t;

		// Find the Secondary Curve
		state.preparse_detailed_block(edid + 0x36);
		state.preparse_detailed_block(edid + 0x48);
		state.preparse_detailed_block(edid + 0x5a);
		state.preparse_detailed_block(edid + 0x6c);

		t = state.calc_gtf_mode(gtf_data.w, gtf_data.h, gtf_data.freq,
					gtf_data.interlaced, gtf_data.ip_parm,
					gtf_data.overscan);
		unsigned hbl = t.hfp + t.hsync + t.hbp;
		unsigned htotal = t.hact + hbl;
		double hor_freq_khz = htotal ? (double)t.pixclk_khz / htotal : 0;

		if (state.base.supports_sec_gtf &&
		    hor_freq_khz >= state.base.sec_gtf_start_freq) {
			t = state.calc_gtf_mode(gtf_data.w, gtf_data.h, gtf_data.freq,
						gtf_data.interlaced, gtf_data.ip_parm,
						gtf_data.overscan, true,
						state.base.C, state.base.M,
						state.base.K, state.base.J);
		}
		calc_ratio(&t);
		if (t.hfp <= 0)
			state.print_timings("", &t, "GTF", "INVALID: Hfront <= 0", true, false);
		else
			state.print_timings("", &t, "GTF", "", true, false);
		return 0;
	}

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
	options[OptPreferredTimings] = 1;
	options[OptNativeTimings] = 1;
	state = edid_state();
	int ret = edid_from_file(input, stderr);
	return ret ? ret : state.parse_edid();
}
#endif
