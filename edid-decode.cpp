// SPDX-License-Identifier: MIT
/*
 * Copyright 2006-2012 Red Hat, Inc.
 * Copyright 2018-2019 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 *
 * Author: Adam Jackson <ajax@nwnk.net>
 * Maintainer: Hans Verkuil <hverkuil-cisco@xs4all.nl>
 */

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

#include <string>

#include "edid-decode.h"

static edid_state state;

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

static std::string s_warn, s_fail;

void warn(const char *fmt, ...)
{
	char buf[256] = "";
	va_list ap;

	va_start(ap, fmt);
	vsprintf(buf, fmt, ap);
	va_end(ap);
	state.warnings++;
	s_warn += std::string(state.cur_block) + ": " + buf;
	if (options[OptCheckInline])
		printf("WARN: %s", buf);
}

void fail(const char *fmt, ...)
{
	char buf[256] = "";
	va_list ap;

	va_start(ap, fmt);
	vsprintf(buf, fmt, ap);
	va_end(ap);
	state.failures++;
	s_fail += std::string(state.cur_block) + ": " + buf;
	if (options[OptCheckInline])
		printf("FAIL: %s", buf);
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
		printf(" (should be 0x%x)\n", -sum & 0xff);
		fail("Invalid checksum\n");
		return;
	}

	printf(" (valid)\n");
}

void print_timings(edid_state &state, const char *prefix,
		   const struct timings *t, const char *suffix)
{
	if (!t) {
		// Should not happen
		fail("unknown short timings\n");
		return;
	}
	state.min_vert_freq_hz = min(state.min_vert_freq_hz, t->refresh);
	state.max_vert_freq_hz = max(state.max_vert_freq_hz, t->refresh);
	state.min_hor_freq_hz = min(state.min_hor_freq_hz, t->hor_freq_hz);
	state.max_hor_freq_hz = max(state.max_hor_freq_hz, t->hor_freq_hz);
	state.max_pixclk_khz = max(state.max_pixclk_khz, t->pixclk_khz);

	printf("%s%ux%u%s@%u %s%u:%u HorFreq: %.3f kHz Clock: %.3f MHz%s\n",
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

void hex_block(const char *prefix, const unsigned char *x,
	       unsigned length, bool show_ascii)
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
		if (show_ascii) {
			printf(" ");
			for (j = 0; j < 16 && i + j < length; j++)
				printf("%c", x[i + j] >= ' ' && x[i + j] <= '~' ? x[i + j] : '.');
		}
		printf("\n");
	}
}

static int edid_lines = 0;

static unsigned char *extract_edid(int fd)
{
	char *ret;
	char *start, *c;
	unsigned char *out = NULL;
	unsigned state = 0;
	unsigned lines = 0;
	int i;
	unsigned out_index = 0;
	unsigned len, size;

	size = 1 << 10;
	ret = (char *)malloc(size);
	len = 0;

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
			t = (char *)realloc(ret, size);
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

			s = (char *)realloc(out, lines * 16);
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
				out = (unsigned char *)realloc(out, out_index + 128);
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

	start = strstr(ret, "EDID (hex):");
	if (start)
		start += 12;
	else
		start = ret;

	/* Is the EDID provided in hex? */
	for (i = 0; i < 32 && (isspace(start[i]) || start[i] == ',' ||
			       tolower(start[i]) == 'x' || isxdigit(start[i])); i++);

	if (i == 32) {
		out = (unsigned char *)malloc(size >> 1);
		if (out == NULL) {
			free(ret);
			return NULL;
		}

		for (c = start; *c; c++) {
			char buf[3];

			if (isspace(*c) || strchr(",:;", *c))
				continue;

			if (*c == '0' && tolower(c[1]) == 'x') {
				c++;
				continue;
			}

			/* Read a %02x from the log */
			if (!isxdigit(c[0]) || !isxdigit(c[1])) {
				if (out_index && out_index % 128 == 0)
					break;
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
			s = (char *)realloc(out, lines * 16);
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

/* generic extension code */

std::string block_name(unsigned char block)
{
	char buf[10];

	switch (block) {
	case 0x02:
		return "CTA-861 Extension Block";
	case 0x10:
		return "VTB Extension Block";
	case 0x40:
		return "Display Information Extension Block";
	case 0x50:
		return "Localized String Extension Block";
	case 0x70:
		return "Display ID Extension Block";
	case 0xf0:
		return "Block Map Extension Block";
	case 0xff:
		return "Manufacturer-Specific Extension Block";
	default:
		sprintf(buf, " (0x%02x)", block);
		return std::string("Unknown Extension Block") + buf;
	}
}

static void parse_block_map(edid_state &state, const unsigned char *x)
{
	static bool saw_block_1;
	unsigned last_valid_block_tag = 0;
	bool fail_once = false;
	unsigned offset = 1;
	unsigned i;

	if (state.cur_block_nr == 1)
		saw_block_1 = true;
	else if (!saw_block_1)
		fail("no EDID Block Map Extension found in block 1\n");

	if (state.cur_block_nr > 1)
		offset = 128;

	for (i = 1; i < 127; i++) {
		unsigned block = offset + i;

		if (x[i]) {
			last_valid_block_tag++;
			if (i != last_valid_block_tag && !fail_once) {
				fail("valid block tags are not consecutive\n");
				fail_once = true;
			}
			printf("  Block %3u: %s\n", block, block_name(block).c_str());
			if (block >= state.num_blocks && !fail_once) {
				fail("invalid block number\n");
				fail_once = true;
			}
		}
	}
}

static void parse_extension(edid_state &state, const unsigned char *x)
{
	state.cur_block = block_name(x[0]);

	printf("\n");
	printf("%s\n", state.cur_block.c_str());
	printf("Extension version: %u\n", x[1]);

	switch (x[0]) {
	case 0x02:
		parse_cta_block(state, x);
		break;
	case 0x20:
		fail("Deprecated extension block, do not use\n");
		break;
	case 0x70:
		parse_displayid_block(state, x);
		break;
	case 0xf0:
		parse_block_map(state, x);
		if (state.cur_block_nr != 1 && state.cur_block_nr != 128)
			fail("must be used in block 1 and 128\n");
		break;
	default:
		hex_block("  ", x + 2, 125);
		break;
	}

	state.cur_block = block_name(x[0]);
	do_checksum("", x, EDID_PAGE_SIZE);
}

static int edid_from_file(const char *from_file, const char *to_file,
			  enum output_format out_fmt)
{
	int fd;
	FILE *out = NULL;
	unsigned char *edid;
	unsigned char *x;
	unsigned i;

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
		if (out != stdout)
			fclose(out);
		return 0;
	}

	if (!edid || memcmp(edid, "\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00", 8)) {
		fprintf(stderr, "No header found\n");
		return -1;
	}

	state.num_blocks = edid_lines / 8;
	if (!options[OptSkipHexDump]) {
		printf("EDID (hex):\n\n");
		for (i = 0; i < state.num_blocks; i++) {
			hex_block("", edid + i * EDID_PAGE_SIZE, EDID_PAGE_SIZE, false);
			printf("\n");
		}
		printf("----------------\n\n");
	}

	if (options[OptExtract])
		dump_breakdown(edid);

	parse_base_block(state, edid);

	x = edid;
	for (i = 1; i < state.num_blocks; i++) {
		x += EDID_PAGE_SIZE;
		state.cur_block_nr++;
		printf("\n----------------\n");
		parse_extension(state, x);
	}

	state.cur_block = "EDID";
	if (state.uses_gtf && !state.supports_gtf)
		fail("GTF timings are used, but the EDID does not signal GTF support\n");
	if (state.uses_cvt && !state.supports_cvt)
		fail("CVT timings are used, but the EDID does not signal CVT support\n");
	if (state.has_display_range_descriptor &&
	    (state.min_vert_freq_hz < state.min_display_vert_freq_hz ||
	     state.max_vert_freq_hz > state.max_display_vert_freq_hz ||
	     state.min_hor_freq_hz < state.min_display_hor_freq_hz ||
	     state.max_hor_freq_hz > state.max_display_hor_freq_hz ||
	     state.max_pixclk_khz > state.max_display_pixclk_khz)) {
		/*
		 * EDID 1.4 states (in an Errata) that explicitly defined
		 * timings supersede the monitor range definition.
		 */
		if (state.edid_minor < 4) {
			fail("\n  One or more of the timings is out of range of the Monitor Ranges:\n"
			     "    Vertical Freq: %u - %u Hz (Monitor: %u - %u Hz)\n"
			     "    Horizontal Freq: %u - %u Hz (Monitor: %u - %u Hz)\n"
			     "    Maximum Clock: %.3f MHz (Monitor: %.3f MHz)\n",
			     state.min_vert_freq_hz, state.max_vert_freq_hz,
			     state.min_display_vert_freq_hz, state.max_display_vert_freq_hz,
			     state.min_hor_freq_hz, state.max_hor_freq_hz,
			     state.min_display_hor_freq_hz, state.max_display_hor_freq_hz,
			     state.max_pixclk_khz / 1000.0, state.max_display_pixclk_khz / 1000.0);
		} else {
			warn("\n  One or more of the timings is out of range of the Monitor Ranges:\n"
			     "    Vertical Freq: %u - %u Hz (Monitor: %u - %u Hz)\n"
			     "    Horizontal Freq: %u - %u Hz (Monitor: %u - %u Hz)\n"
			     "    Maximum Clock: %.3f MHz (Monitor: %.3f MHz)\n",
			     state.min_vert_freq_hz, state.max_vert_freq_hz,
			     state.min_display_vert_freq_hz, state.max_display_vert_freq_hz,
			     state.min_hor_freq_hz, state.max_hor_freq_hz,
			     state.min_display_hor_freq_hz, state.max_display_hor_freq_hz,
			     state.max_pixclk_khz / 1000.0, state.max_display_pixclk_khz / 1000.0);
		}
	}

	free(edid);
	if (!options[OptCheck] && !options[OptCheckInline])
		return 0;

	printf("\n----------------\n");

	if (options[OptCheck]) {
		if (state.warnings)
			printf("\nWarnings:\n\n%s", s_warn.c_str());
		if (state.failures)
			printf("\nFailures:\n\n%s", s_fail.c_str());
	}
	printf("\nEDID conformity: %s\n", state.failures ? "FAIL" : "PASS");
	return state.failures ? -2 : 0;
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
