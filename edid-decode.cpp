// SPDX-License-Identifier: MIT
/*
 * Copyright 2006-2012 Red Hat, Inc.
 * Copyright 2018-2019 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 *
 * Author: Adam Jackson <ajax@nwnk.net>
 * Maintainer: Hans Verkuil <hverkuil-cisco@xs4all.nl>
 */

#include <ctype.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "edid-decode.h"
#include "version.h"

static edid_state state;

static unsigned char edid[EDID_PAGE_SIZE * EDID_MAX_BLOCKS];

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
	OptOutputFormat = 'o',
	OptSkipHexDump = 's',
	OptLast = 256
};

static char options[OptLast];

static struct option long_options[] = {
	{ "help", no_argument, 0, OptHelp },
	{ "output-format", required_argument, 0, OptOutputFormat },
	{ "extract", no_argument, 0, OptExtract },
	{ "skip-hex-dump", no_argument, 0, OptSkipHexDump },
	{ "check-inline", no_argument, 0, OptCheckInline },
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
	       "  -c, --check           check if the EDID conforms to the standards, failures and\n"
	       "                        warnings are reported at the end.\n"
	       "  -C, --check-inline    check if the EDID conforms to the standards, failures and\n"
	       "                        warnings are reported inline.\n"
	       "  -s, --skip-hex-dump   skip the initial hex dump of the EDID\n"
	       "  -e, --extract         extract the contents of the first block in hex values\n"
	       "  -h, --help            display this help message\n");
}

static std::string s_msgs[EDID_MAX_BLOCKS + 1][2];

void msg(bool is_warn, const char *fmt, ...)
{
	char buf[256] = "";
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
		printf("Block %u (%s):\n%s",
		       i, block_name(edid[i * EDID_PAGE_SIZE]).c_str(),
		       s_msgs[i][is_warn].c_str());
	}
	if (s_msgs[EDID_MAX_BLOCKS][is_warn].empty())
		return;
	printf("All Blocks:\n%s",
	       s_msgs[EDID_MAX_BLOCKS][is_warn].c_str());
}


void do_checksum(const char *prefix, const unsigned char *x, size_t len)
{
	unsigned char check = x[len - 1];
	unsigned char sum = 0;
	unsigned i;

	printf("%sChecksum: 0x%hhx", prefix, check);

	for (i = 0; i < len-1; i++)
		sum += x[i];

	if ((unsigned char)(check + sum) != 0) {
		printf(" (should be 0x%02x)\n", -sum & 0xff);
		fail("Invalid checksum 0x%02x (should be 0x%02x)\n",
		     check, -sum & 0xff);
		return;
	}
	printf("\n");
}

void edid_state::print_timings(const char *prefix,
			       const struct timings *t, const char *suffix)
{
	if (!t) {
		// Should not happen
		fail("Unknown short timings\n");
		return;
	}
	min_vert_freq_hz = min(min_vert_freq_hz, t->refresh);
	max_vert_freq_hz = max(max_vert_freq_hz, t->refresh);
	min_hor_freq_hz = min(min_hor_freq_hz, t->hor_freq_hz);
	max_hor_freq_hz = max(max_hor_freq_hz, t->hor_freq_hz);
	max_pixclk_khz = max(max_pixclk_khz, t->pixclk_khz);

	std::string s(suffix);
	if (t->rb) {
		if (s.empty())
			s = "RB";
		else
			s += ", RB";
	}
	if (!s.empty())
		s = " (" + s + ")";

	char buf[10];
	sprintf(buf, "%u%s", t->h, t->interlaced ? "i" : "");
	printf("%s%5ux%-5s %3u Hz %3u:%-3u %7.3f kHz %7.3f MHz%s\n",
	       prefix,
	       t->w, buf,
	       t->refresh,
	       t->ratio_w, t->ratio_h,
	       t->hor_freq_hz / 1000.0,
	       t->pixclk_khz / 1000.0,
	       s.c_str());
}

std::string utohex(unsigned char x)
{
	char buf[10];

	sprintf(buf, "0x%02hhx", x);
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
		printf("%s", prefix);
		for (j = 0; j < step; j++)
			if (i + j < length)
				printf("%02x ", x[i + j]);
			else if (length > step)
				printf("   ");
		if (show_ascii) {
			printf(" ");
			for (j = 0; j < step && i + j < length; j++)
				printf("%c", x[i + j] >= ' ' && x[i + j] <= '~' ? x[i + j] : '.');
		}
		printf("\n");
	}
}

static bool edid_add_byte(const char *s)
{
	char buf[3];

	if (state.edid_size == sizeof(edid))
		return false;
	buf[0] = s[0];
	buf[1] = s[1];
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
	return state.edid_size && !(state.edid_size % EDID_PAGE_SIZE);
}

static const char *ignore_chars = ",:;";

static bool extract_edid_hex(const char *s)
{
	for (; *s; s++) {
		if (isspace(*s) || strchr(ignore_chars, *s))
			continue;

		if (*s == '0' && tolower(s[1]) == 'x') {
			s++;
			continue;
		}

		/* Read a %02x from the log */
		if (!isxdigit(s[0]) || !isxdigit(s[1])) {
			if (state.edid_size && state.edid_size % 128 == 0)
				break;
			return false;
		}
		if (!edid_add_byte(s))
			return false;
		s++;
	}
	return state.edid_size && !(state.edid_size % EDID_PAGE_SIZE);
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
	return state.edid_size && !(state.edid_size % EDID_PAGE_SIZE);
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
	return state.edid_size && !(state.edid_size % EDID_PAGE_SIZE);
}

static bool extract_edid(int fd)
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
		return extract_edid_hex(strchr(start, '{') + 1);

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
	if (edid_data.size() % EDID_PAGE_SIZE || edid_data.size() > sizeof(edid))
		return false;
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
	FILE *out = NULL;
	int fd;

	if (!from_file || !strcmp(from_file, "-")) {
		from_file = "stdin";
		fd = 0;
	} else if ((fd = open(from_file, O_RDONLY)) == -1) {
		perror(from_file);
		return -1;
	}
	if (to_file) {
		if (!strcmp(to_file, "-")) {
			to_file = "stdout";
			out = stdout;
		} else if ((out = fopen(to_file, "w")) == NULL) {
			perror(to_file);
			return -1;
		}
		if (out_fmt == OUT_FMT_DEFAULT)
			out_fmt = out == stdout ? OUT_FMT_HEX : OUT_FMT_RAW;
	}

	if (!extract_edid(fd)) {
		fprintf(stderr, "EDID extract of '%s' failed\n", from_file);
		return -1;
	}
	state.num_blocks = state.edid_size / EDID_PAGE_SIZE;
	if (fd != 0)
		close(fd);

	if (memcmp(edid, "\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00", 8)) {
		fprintf(stderr, "No EDID header found in '%s'\n", from_file);
		return -1;
	}

	if (out) {
		write_edid(out, edid, state.edid_size, out_fmt);
		if (out != stdout)
			fclose(out);
		return 0;
	}

	return 0;
}

/* generic extension code */

std::string block_name(unsigned char block)
{
	char buf[10];

	switch (block) {
	case 0x00: return "Base Block";
	case 0x02: return "CTA-861 Extension Block";
	case 0x10: return "VTB Extension Block";
	case 0x40: return "Display Information Extension Block";
	case 0x50: return "Localized String Extension Block";
	case 0x70: return "Display ID Extension Block";
	case 0xf0: return "Block Map Extension Block";
	case 0xff: return "Manufacturer-Specific Extension Block";
	default:
		sprintf(buf, " 0x%02x", block);
		return std::string("Unknown EDID Extension Block") + buf;
	}
}

void edid_state::parse_block_map(const unsigned char *x)
{
	static bool saw_block_1;
	unsigned last_valid_block_tag = 0;
	bool fail_once = false;
	unsigned offset = 1;
	unsigned i;

	printf("%s\n", block.c_str());
	if (block_nr == 1)
		saw_block_1 = true;
	else if (!saw_block_1)
		fail("No EDID Block Map Extension found in block 1\n");

	if (block_nr > 1)
		offset = 128;

	for (i = 1; i < 127; i++) {
		unsigned block = offset + i;

		if (x[i]) {
			last_valid_block_tag++;
			if (i != last_valid_block_tag && !fail_once) {
				fail("Valid block tags are not consecutive\n");
				fail_once = true;
			}
			printf("  Block %3u: %s\n", block, block_name(block).c_str());
			if (block >= num_blocks && !fail_once) {
				fail("Invalid block number %u\n", block);
				fail_once = true;
			}
		}
	}
}

void edid_state::parse_extension(const unsigned char *x)
{
	block = block_name(x[0]);
	data_block.clear();

	printf("\n");

	switch (x[0]) {
	case 0x02:
		parse_cta_block(x);
		break;
	case 0x20:
		printf("%s\n", block.c_str());
		fail("Deprecated extension block, do not use\n");
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
			fail("Must be used in block 1 and 128\n");
		break;
	default:
		printf("%s\n", block.c_str());
		hex_block("  ", x, EDID_PAGE_SIZE);
		fail("Unknown Extension Block\n");
		break;
	}

	data_block.clear();
	do_checksum("", x, EDID_PAGE_SIZE);
}

int edid_state::parse_edid()
{
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
	parse_base_block(edid);

	for (unsigned i = 1; i < num_blocks; i++) {
		block_nr++;
		printf("\n----------------\n");
		parse_extension(edid + i * EDID_PAGE_SIZE);
	}

	block = "";
	block_nr = EDID_MAX_BLOCKS;
	if (uses_gtf && !supports_gtf)
		fail("GTF timings are used, but the EDID does not signal GTF support\n");
	if (uses_cvt && !supports_cvt)
		fail("CVT timings are used, but the EDID does not signal CVT support\n");
	if (has_display_range_descriptor &&
	    (min_vert_freq_hz < min_display_vert_freq_hz ||
	     max_vert_freq_hz > max_display_vert_freq_hz ||
	     min_hor_freq_hz < min_display_hor_freq_hz ||
	     max_hor_freq_hz > max_display_hor_freq_hz ||
	     max_pixclk_khz > max_display_pixclk_khz)) {
		/*
		 * EDID 1.4 states (in an Errata) that explicitly defined
		 * timings supersede the monitor range definition.
		 */
		if (edid_minor < 4) {
			fail("One or more of the timings is out of range of the Monitor Ranges:\n"
			     "    Vertical Freq: %u - %u Hz (Monitor: %u - %u Hz)\n"
			     "    Horizontal Freq: %u - %u Hz (Monitor: %u - %u Hz)\n"
			     "    Maximum Clock: %.3f MHz (Monitor: %.3f MHz)\n",
			     min_vert_freq_hz, max_vert_freq_hz,
			     min_display_vert_freq_hz, max_display_vert_freq_hz,
			     min_hor_freq_hz, max_hor_freq_hz,
			     min_display_hor_freq_hz, max_display_hor_freq_hz,
			     max_pixclk_khz / 1000.0, max_display_pixclk_khz / 1000.0);
		} else {
			warn("One or more of the timings is out of range of the Monitor Ranges:\n"
			     "    Vertical Freq: %u - %u Hz (Monitor: %u - %u Hz)\n"
			     "    Horizontal Freq: %u - %u Hz (Monitor: %u - %u Hz)\n"
			     "    Maximum Clock: %.3f MHz (Monitor: %.3f MHz)\n",
			     min_vert_freq_hz, max_vert_freq_hz,
			     min_display_vert_freq_hz, max_display_vert_freq_hz,
			     min_hor_freq_hz, max_hor_freq_hz,
			     min_display_hor_freq_hz, max_display_hor_freq_hz,
			     max_pixclk_khz / 1000.0, max_display_pixclk_khz / 1000.0);
		}
	}
	if (svds.size() <= y420cmdb_max_idx)
		fail("YCbCr 4:2:0 Capability Map Data Block max index %u >= %u (#SVDs)\n",
		     y420cmdb_max_idx, svds.size());

	if (!options[OptCheck] && !options[OptCheckInline])
		return 0;

	printf("\n----------------\n\n");

#ifdef SHA
#define STR(x) #x
#define STRING(x) STR(x)
	printf("edid-decode SHA: %s\n", STRING(SHA));
#else
	printf("edid-decode SHA: not available\n");
#endif

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
		ret = edid_from_file(NULL, NULL, out_fmt);
	else if (optind == argc - 1)
		ret = edid_from_file(argv[optind], NULL, out_fmt);
	else
		return edid_from_file(argv[optind], argv[optind + 1], out_fmt);
	return ret ? ret : state.parse_edid();
}
