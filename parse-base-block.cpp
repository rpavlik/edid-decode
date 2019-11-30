// SPDX-License-Identifier: MIT
/*
 * Copyright 2006-2012 Red Hat, Inc.
 * Copyright 2018-2019 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 *
 * Author: Adam Jackson <ajax@nwnk.net>
 * Maintainer: Hans Verkuil <hverkuil-cisco@xs4all.nl>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "edid-decode.h"

static char *manufacturer_name(const unsigned char *x)
{
	static char name[4];

	name[0] = ((x[0] & 0x7c) >> 2) + '@';
	name[1] = ((x[0] & 0x03) << 3) + ((x[1] & 0xe0) >> 5) + '@';
	name[2] = (x[1] & 0x1f) + '@';
	name[3] = 0;

	if (!isupper(name[0]) || !isupper(name[1]) || !isupper(name[2]))
		fail("Manufacturer name field contains garbage\n");

	return name;
}

static const struct {
	unsigned dmt_id;
	unsigned std_id;
	unsigned cvt_id;
	struct timings t;
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

	{ 0x52, 0xd1c0, 0x000000, { 1920, 1080, 60, 16, 9, 67500, 148500 } },

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

const struct timings *find_dmt_id(unsigned char dmt_id)
{
	unsigned i;

	for (i = 0; i < ARRAY_SIZE(dmt_timings); i++)
		if (dmt_timings[i].dmt_id == dmt_id)
			return &dmt_timings[i].t;
	return NULL;
}

static const struct timings *find_std_id(unsigned short std_id)
{
	unsigned i;

	for (i = 0; i < ARRAY_SIZE(dmt_timings); i++)
		if (dmt_timings[i].std_id == std_id)
			return &dmt_timings[i].t;
	return NULL;
}

/*
 * Copied from xserver/hw/xfree86/modes/xf86gtf.c
 */
static void edid_gtf_mode(edid_state &state, const char *prefix, struct timings *t)
{
#define CELL_GRAN         8.0   /* assumed character cell granularity        */
#define MIN_PORCH         1     /* minimum front porch                       */
#define V_SYNC_RQD        3     /* width of vsync in lines                   */
#define H_SYNC_PERCENT    8.0   /* width of hsync as % of total line         */
#define MIN_VSYNC_PLUS_BP 550.0 /* min time of vsync + back porch (microsec) */
#define M                 600.0 /* blanking formula gradient                 */
#define C                 40.0  /* blanking formula offset                   */
#define K                 128.0 /* blanking formula scaling factor           */
#define J                 20.0  /* blanking formula scaling factor           */

	/* C' and M' are part of the Blanking Duty Cycle computation */

#define C_PRIME           (((C - J) * K/256.0) + J)
#define M_PRIME           (K/256.0 * M)
	float h_pixels_rnd;
	float v_lines_rnd;
	float v_field_rate_rqd;
	float interlace;
	float h_period_est;
	float vsync_plus_bp;
	float total_v_lines;
	float v_field_rate_est;
	float h_period;
	float total_active_pixels;
	float ideal_duty_cycle;
	float h_blank;
	float total_pixels;

	/*  1. In order to give correct results, the number of horizontal
	 *  pixels requested is first processed to ensure that it is divisible
	 *  by the character size, by rounding it to the nearest character
	 *  cell boundary:
	 *
	 *  [H PIXELS RND] = ((ROUND([H PIXELS]/[CELL GRAN RND],0))*[CELLGRAN RND])
	 */

	h_pixels_rnd = rint((float)t->x / CELL_GRAN) * CELL_GRAN;

	/*  2. If interlace is requested, the number of vertical lines assumed
	 *  by the calculation must be halved, as the computation calculates
	 *  the number of vertical lines per field. In either case, the
	 *  number of lines is rounded to the nearest integer.
	 *
	 *  [V LINES RND] = IF([INT RQD?]="y", ROUND([V LINES]/2,0),
	 *                                     ROUND([V LINES],0))
	 */

	v_lines_rnd = t->interlaced ?
		rint((float)t->y) / 2.0 :
		rint((float)t->y);

	/*  3. Find the frame rate required:
	 *
	 *  [V FIELD RATE RQD] = IF([INT RQD?]="y", [I/P FREQ RQD]*2,
	 *                                          [I/P FREQ RQD])
	 */

	v_field_rate_rqd = t->interlaced ? (t->refresh * 2.0) : (t->refresh);

	/*  6. If interlace is required, then set variable [INTERLACE]=0.5:
	 *
	 *  [INTERLACE]=(IF([INT RQD?]="y",0.5,0))
	 */

	interlace = t->interlaced ? 0.5 : 0.0;

	/*  7. Estimate the Horizontal period
	 *
	 *  [H PERIOD EST] = ((1/[V FIELD RATE RQD]) - [MIN VSYNC+BP]/1000000) /
	 *                    ([V LINES RND] +
	 *                     [MIN PORCH RND]+[INTERLACE]) * 1000000
	 */

	h_period_est = (((1.0/v_field_rate_rqd) - (MIN_VSYNC_PLUS_BP/1000000.0))
			/ (v_lines_rnd + MIN_PORCH + interlace)
			* 1000000.0);

	/*  8. Find the number of lines in V sync + back porch:
	 *
	 *  [V SYNC+BP] = ROUND(([MIN VSYNC+BP]/[H PERIOD EST]),0)
	 */

	vsync_plus_bp = rint(MIN_VSYNC_PLUS_BP/h_period_est);

	/*  10. Find the total number of lines in Vertical field period:
	 *
	 *  [TOTAL V LINES] = [V LINES RND] +
	 *                    [V SYNC+BP] + [INTERLACE] +
	 *                    [MIN PORCH RND]
	 */

	total_v_lines = v_lines_rnd + vsync_plus_bp +
		interlace + MIN_PORCH;

	/*  11. Estimate the Vertical field frequency:
	 *
	 *  [V FIELD RATE EST] = 1 / [H PERIOD EST] / [TOTAL V LINES] * 1000000
	 */

	v_field_rate_est = 1.0 / h_period_est / total_v_lines * 1000000.0;

	/*  12. Find the actual horizontal period:
	 *
	 *  [H PERIOD] = [H PERIOD EST] / ([V FIELD RATE RQD] / [V FIELD RATE EST])
	 */

	h_period = h_period_est / (v_field_rate_rqd / v_field_rate_est);

	/*  17. Find total number of active pixels in image
	 *
	 *  [TOTAL ACTIVE PIXELS] = [H PIXELS RND]
	 */

	total_active_pixels = h_pixels_rnd;

	/*  18. Find the ideal blanking duty cycle from the blanking duty cycle
	 *  equation:
	 *
	 *  [IDEAL DUTY CYCLE] = [C'] - ([M']*[H PERIOD]/1000)
	 */

	ideal_duty_cycle = C_PRIME - (M_PRIME * h_period / 1000.0);

	/*  19. Find the number of pixels in the blanking time to the nearest
	 *  double character cell:
	 *
	 *  [H BLANK (PIXELS)] = (ROUND(([TOTAL ACTIVE PIXELS] *
	 *                               [IDEAL DUTY CYCLE] /
	 *                               (100-[IDEAL DUTY CYCLE]) /
	 *                               (2*[CELL GRAN RND])), 0))
	 *                       * (2*[CELL GRAN RND])
	 */

	h_blank = rint(total_active_pixels *
		       ideal_duty_cycle /
		       (100.0 - ideal_duty_cycle) /
		       (2.0 * CELL_GRAN)) * (2.0 * CELL_GRAN);

	/*  20. Find total number of pixels:
	 *
	 *  [TOTAL PIXELS] = [TOTAL ACTIVE PIXELS] + [H BLANK (PIXELS)]
	 */

	total_pixels = total_active_pixels + h_blank;

	/*  21. Find pixel clock frequency:
	 *
	 *  [PIXEL FREQ] = [TOTAL PIXELS] / [H PERIOD]
	 */

	t->pixclk_khz = (int)(1000.0 * total_pixels / h_period);

	/*  22. Find horizontal frequency:
	 *
	 *  [H FREQ] = 1000 / [H PERIOD]
	 */

	t->hor_freq_hz = 1000000.0 / h_period;

	print_timings(state, prefix, t, "GTF");
}

/*
 * Copied from xserver/hw/xfree86/modes/xf86cvt.c
 */
static void edid_cvt_mode(edid_state &state, const char *prefix,
			  struct timings *t, int preferred)
{
	int HDisplay = t->x;
	int VDisplay = t->y;

	/* character cell horizontal granularity (pixels) - default 8 */
#define CVT_H_GRANULARITY 8

	/* Minimum vertical porch (lines) - default 3 */
#define CVT_MIN_V_PORCH 3

	/* Minimum number of vertical back porch lines - default 6 */
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

	print_timings(state, prefix, t, preferred ? "CVT, preferred vertical rate" : "CVT");
}

static void detailed_cvt_descriptor(edid_state &state, const unsigned char *x, int first)
{
	static const unsigned char empty[3] = { 0, 0, 0 };
	struct timings cvt_t = {};
	unsigned char preferred;

	if (!first && !memcmp(x, empty, 3))
		return;

	state.uses_cvt = true;
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
		edid_cvt_mode(state, "    ", &cvt_t, preferred == 0);
	}
	if (x[2] & 0x08) {
		cvt_t.refresh = 60;
		edid_cvt_mode(state, "    ", &cvt_t, preferred == 1);
	}
	if (x[2] & 0x04) {
		cvt_t.refresh = 75;
		edid_cvt_mode(state, "    ", &cvt_t, preferred == 2);
	}
	if (x[2] & 0x02) {
		cvt_t.refresh = 85;
		edid_cvt_mode(state, "    ", &cvt_t, preferred == 3);
	}
	if (x[2] & 0x01) {
		cvt_t.refresh = 60;
		cvt_t.rb = 1;
		edid_cvt_mode(state, "    ", &cvt_t, preferred == 4);
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
					fail("Empty string\n");
				else if (s[i - 1] == 0x20)
					fail("One or more trailing spaces\n");
			} else if (x[i] == 0x20) {
				s[i] = x[i];
			} else {
				fail("Non-printable character\n");
				return s;
			}
		} else if (x[i] != 0x20) {
			fail("Non-space after newline\n");
			return s;
		}
	}
	/* Does the string end with a space? */
	if (!seen_newline && s[len - 1] == 0x20)
		fail("One or more trailing spaces\n");

	return s;
}

static const struct {
	unsigned dmt_id;
	struct timings t;
	const char *std_name;
} established_timings12[] = {
	// For IBM formats see: http://www.mcamafia.de/pdf/pdfref.htm:
	// VGA / XGA / XGA-2 Technical Reference Manual
	//
	// For Apple formats (mac6, mac13 and mac18) see
	// drivers/video/fbdev/macmodes.c in the linux kernel.

	/* 0x23 bit 7 - 0 */
	// 720x400p70: +vsync -hsync 
	{ 0x00, { 720, 400, 70, 9, 5, 31469, 28250 }, "IBM" },
	{ 0x00, { 720, 400, 88, 9, 5, 39500, 35500 }, "IBM" },
	{ 0x04 },
	{ 0x00, { 640, 480, 67, 4, 3, 35000, 30240 }, "Apple" },
	{ 0x05 },
	{ 0x06 },
	{ 0x08 },
	{ 0x09 },
	/* 0x24 bit 7 - 0 */
	{ 0x0a },
	{ 0x0b },
	{ 0x00, { 832, 624, 75, 4, 3, 49107, 55000 }, "Apple" },
	{ 0x00, { 1024, 768, 87, 4, 3, 35522, 44900, 0, 1 }, "IBM" },
	{ 0x10 },
	{ 0x11 },
	{ 0x12 },
	{ 0x24 },
	/* 0x25 bit 7 */
	{ 0x00, { 1152, 870, 75, 192, 145, 67500, 108000 }, "Apple" },
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

static void print_standard_timing(edid_state &state, uint8_t b1, uint8_t b2)
{
	const struct timings *t;
	struct timings formula = {};
	unsigned ratio_w, ratio_h;
	unsigned x, y, refresh;
	unsigned i;

	if (b1 == 0x01 && b2 == 0x01)
		return;

	if (b1 == 0) {
		fail("Non-conformant standard timing (0 horiz)\n");
		return;
	}
	t = find_std_id((b1 << 8) | b2);
	if (t) {
		print_timings(state, "  ", t, "DMT");
		return;
	}
	x = (b1 + 31) * 8;
	switch ((b2 >> 6) & 0x3) {
	case 0x00:
		if (state.edid_minor >= 3) {
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

	state.min_vert_freq_hz = min(state.min_vert_freq_hz, refresh);
	state.max_vert_freq_hz = max(state.max_vert_freq_hz, refresh);
	for (i = 0; i < ARRAY_SIZE(established_timings12); i++) {
		const char *suffix = "DMT";

		if (established_timings12[i].dmt_id) {
			t = find_dmt_id(established_timings12[i].dmt_id);
		} else {
			t = &established_timings12[i].t;
			suffix = established_timings12[i].std_name;
		}
		if (t->x == x && t->y == y && t->refresh == refresh &&
		    t->ratio_w == ratio_w && t->ratio_h == ratio_h) {
			print_timings(state, "  ", t, suffix);
			return;
		}
	}
	for (i = 0; i < ARRAY_SIZE(dmt_timings); i++) {
		t = &dmt_timings[i].t;

		if (t->x == x && t->y == y &&
		    t->refresh == refresh &&
		    t->ratio_w == ratio_w && t->ratio_h == ratio_h) {
			print_timings(state, "  ", t, "DMT");
			return;
		}
	}

	formula.x = x;
	formula.y = y;
	formula.refresh = refresh;
	formula.ratio_w = ratio_w;
	formula.ratio_h = ratio_h;

	if (state.edid_minor >= 4) {
		state.uses_cvt = true;
		edid_cvt_mode(state, "  ", &formula, 0);
		/*
		 * A EDID 1.3 source will assume GTF, so both GTF and CVT
		 * have to be supported.
		 */
		state.uses_gtf = true;
		edid_gtf_mode(state, "  ", &formula);
	} else if (state.edid_minor >= 2) {
		state.uses_gtf = true;
		edid_gtf_mode(state, "  ", &formula);
	} else {
		printf("  %ux%u@%u %u:%u\n",
		       x, y, refresh, ratio_w, ratio_h);
	}
}

static void detailed_display_range_limits(edid_state &state, const unsigned char *x)
{
	int h_max_offset = 0, h_min_offset = 0;
	int v_max_offset = 0, v_min_offset = 0;
	int is_cvt = 0;
	bool has_sec_gtf = false;
	std::string range_class;

	state.cur_block = "Display Range Limits";
	printf("%s\n", state.cur_block.c_str());
	state.has_display_range_descriptor = 1;
	/* 
	 * XXX todo: implement feature flags, vtd blocks
	 * XXX check: ranges are well-formed; block termination if no vtd
	 */
	if (state.edid_minor >= 4) {
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
		if (state.edid_minor >= 4 && !state.supports_continuous_freq)
			fail("GTF can't be combined with non-continuous frequencies\n");
		state.supports_gtf = true;
		break;
	case 0x01: /* range limits only */
		range_class = "Bare Limits";
		if (state.edid_minor < 4)
			fail("'%s' is not allowed for EDID < 1.4\n", range_class);
		break;
	case 0x02: /* secondary gtf curve */
		range_class = "Secondary GTF";
		if (state.edid_minor >= 4 && !state.supports_continuous_freq)
			fail("GTF can't be combined with non-continuous frequencies\n");
		state.supports_gtf = true;
		has_sec_gtf = true;
		break;
	case 0x04: /* cvt */
		range_class = "CVT";
		is_cvt = 1;
		if (state.edid_minor < 4)
			fail("'%s' is not allowed for EDID < 1.4\n", range_class);
		else if (!state.supports_continuous_freq)
			fail("CVT can't be combined with non-continuous frequencies\n");
		if (state.edid_minor >= 4) {
			/* GTF is implied if CVT is signaled */
			state.supports_gtf = true;
			state.supports_cvt = true;
		}
		break;
	default: /* invalid */
		fail("Unknown range class (0x%02x)\n", x[10]);
		range_class = std::string("Unknown (") + utohex(x[10]) + ")";
		break;
	}

	if (x[5] + v_min_offset > x[6] + v_max_offset)
		fail("Min vertical rate > max vertical rate\n");
	state.min_display_vert_freq_hz = x[5] + v_min_offset;
	state.max_display_vert_freq_hz = x[6] + v_max_offset;
	if (x[7] + h_min_offset > x[8] + h_max_offset)
		fail("Min horizontal freq > max horizontal freq\n");
	state.min_display_hor_freq_hz = (x[7] + h_min_offset) * 1000;
	state.max_display_hor_freq_hz = (x[8] + h_max_offset) * 1000;
	printf("  Monitor ranges (%s): %d-%d Hz V, %d-%d kHz H",
	       range_class.c_str(),
	       x[5] + v_min_offset, x[6] + v_max_offset,
	       x[7] + h_min_offset, x[8] + h_max_offset);
	if (x[9]) {
		state.max_display_pixclk_khz = x[9] * 10000;
		printf(", max dotclock %d MHz\n", x[9] * 10);
	} else {
		if (state.edid_minor >= 4)
			fail("EDID 1.4 block does not set max dotclock\n");
		printf("\n");
	}

	if (has_sec_gtf) {
		if (x[11])
			fail("Byte 11 is 0x%02x instead of 0x00\n", x[11]);
		printf("  GTF Secondary Curve Block\n");
		printf("    Start frequency: %u kHz\n", x[12] * 2);
		printf("    C: %f\n", x[13] / 2.0);
		if (x[13] > 127)
			fail("Byte 13 is > 127\n");
		printf("    M: %u\n", (x[15] << 8) | x[14]);
		printf("    K: %u\n", x[16]);
		printf("    J: %f\n", x[17] / 2.0);
		if (x[17] > 127)
			fail("Byte 17 is > 127\n");
	} else if (is_cvt) {
		int max_h_pixels = 0;

		printf("  CVT version %d.%d\n", (x[11] & 0xf0) >> 4, x[11] & 0x0f);

		if (x[12] & 0xfc) {
			unsigned raw_offset = (x[12] & 0xfc) >> 2;

			printf("  Real max dotclock: %.2f MHz\n",
			       (x[9] * 10) - (raw_offset * 0.25));
			if (raw_offset >= 40)
				warn("CVT block corrects dotclock by more than 9.75 MHz\n");
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
		switch ((x[15] & 0xe0) >> 5) {
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
			printf("Unknown (0x%02x)", (x[15] & 0xe0) >> 5);
			fail("Invalid preferred aspect ratio 0x%02x\n",
			     (x[15] & 0xe0) >> 5);
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
	} else {
		if (x[11] != 0x0a)
			fail("Byte 11 is 0x%02x instead of 0x0a\n", x[11]);
		for (unsigned i = 12; i <= 17; i++) {
			if (x[i] != 0x20) {
				fail("Byte %u is 0x%02x instead of 0x20\n", i, x[i]);
				break;
			}
		}
	}
}

static void detailed_epi(edid_state &state, const unsigned char *x)
{
	state.cur_block = "EPI Descriptor";
	printf("%s\n", state.cur_block.c_str());

	unsigned v = x[5] & 0x07;

	printf("  Bits per pixel: %u\n", 18 + v * 6);
	if (v > 2)
		fail("Invalid bits per pixel\n");
	v = (x[5] & 0x18) >> 3;
	printf("  Pixels per clock: %u\n", 1 << v);
	if (v > 2)
		fail("Invalid pixels per clock\n");
	v = (x[5] & 0x60) >> 5;
	printf("  Data color mapping: %sconventional\n", v ? "non-" : "");
	if (v > 1)
		fail("Unknown data color mapping (0x%02x)\n", v);
	if (x[5] & 0x80)
		fail("Non-zero reserved field in byte 5\n");

	v = x[6] & 0x0f;
	printf("  Interface type: ");
	switch (v) {
	case 0x00: printf("LVDS TFT\n"); break;
	case 0x01: printf("monoSTN 4/8 Bit\n"); break;
	case 0x02: printf("colorSTN 8/16 Bit\n"); break;
	case 0x03: printf("18 Bit TFT\n"); break;
	case 0x04: printf("24 Bit TFT\n"); break;
	case 0x05: printf("TMDS\n"); break;
	default:
		   printf("Unknown (0x%02x)\n", v);
		   fail("Invalid interface type 0x%02x\n", v);
		   break;
	}
	printf("  DE polarity: DE %s active\n",
	       (x[6] & 0x10) ? "low" : "high");
	printf("  FPSCLK polarity: FPSCLK %sinverted\n",
	       (x[6] & 0x20) ? "" : "not ");
	if (x[6] & 0xc0)
		fail("Non-zero reserved field in byte 6\n");

	printf("  Vertical display mode: %s\n",
	       (x[7] & 0x01) ? "Up/Down reverse mode" : "normal");
	printf("  Horizontal display mode: %s\n",
	       (x[7] & 0x02) ? "Left/Right reverse mode" : "normal");
	if (x[7] & 0xfc)
		fail("Non-zero reserved field in byte 7\n");

	v = x[8] & 0x0f;
	printf("  Total power on sequencing delay: ");
	if (v)
		printf("%u ms\n", v * 10);
	else
		printf("VGA controller default\n");
	v = (x[8] & 0xf0) >> 4;
	printf("  Total power off sequencing delay: ");
	if (v)
		printf("%u ms\n", v * 10);
	else
		printf("VGA controller default\n");

	v = x[9] & 0x0f;
	printf("  Contrast power on sequencing delay: ");
	if (v)
		printf("%u ms\n", v * 10);
	else
		printf("VGA controller default\n");
	v = (x[9] & 0xf0) >> 4;
	printf("  Contrast power off sequencing delay: ");
	if (v)
		printf("%u ms\n", v * 10);
	else
		printf("VGA controller default\n");

	v = x[10] & 0x2f;
	const char *s = (x[10] & 0x80) ? "" : " (ignored)";

	printf("  Backlight brightness control: %u steps%s\n", v, s);
	printf("  Backlight enable at boot: %s%s\n",
	       (x[10] & 0x40) ? "off" : "on", s);
	printf("  Backlight control enable: %s\n",
	       (x[10] & 0x80) ? "enabled" : "disabled");

	v = x[11] & 0x2f;
	s = (x[11] & 0x80) ? "" : " (ignored)";

	printf("  Contrast voltable control: %u steps%s\n", v, s);
	if (x[11] & 0x40)
		fail("Non-zero reserved field in byte 11\n");
	printf("  Contrast control enable: %s\n",
	       (x[11] & 0x80) ? "enabled" : "disabled");

	if (x[12] || x[13] || x[14] || x[15] || x[16])
		fail("Non-zero values in reserved bytes 12-16\n");

	printf("  EPI Version: %u.%u\n", (x[17] & 0xf0) >> 4, x[17] & 0x0f);
}

static void add_str(std::string &s, const std::string &add)
{
	if (s.empty())
		s = add;
	else
		s = s + ", " + add;
}

void detailed_timings(edid_state &state, const char *prefix, const unsigned char *x)
{
	unsigned ha, hbl, hso, hspw, hborder, va, vbl, vso, vspw, vborder;
	unsigned hor_mm, vert_mm;
	unsigned pixclk_khz;
	double refresh;
	std::string s_sync, s_flags;

	state.cur_block = "Detailed Timings";
	if (x[0] == 0 && x[1] == 0) {
		fail("First two bytes are 0, invalid data\n");
		return;
	}
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

	unsigned char flags = x[17];

	if (state.has_spwg && state.timing_descr_cnt == 2)
		flags = *(x - 1);

	switch ((flags & 0x18) >> 3) {
	case 0x00:
		s_flags = "analog composite";
		/* fall-through */
	case 0x01:
		if (s_flags.empty())
			s_flags = "bipolar analog composite";
		switch ((flags & 0x06) >> 1) {
		case 0x00:
			add_str(s_flags, "sync-on-green");
			break;
		case 0x01:
			break;
		case 0x02:
			add_str(s_flags, "serrate, sync-on-green");
			break;
		case 0x03:
			add_str(s_flags, "serrate");
			break;
		}
		break;
	case 0x02:
		s_sync = (flags & (1 << 1)) ? "+hsync " : "-hsync ";
		s_flags = "digital composite";
		if (flags & (1 << 2))
		    add_str(s_flags, "serrate");
		break;
	case 0x03:
		s_sync = (flags & (1 << 2)) ? "+vsync " : "-vsync ";
		s_sync += (flags & (1 << 1)) ? "+hsync " : "-hsync ";
		if (state.has_spwg && (flags & 0x01))
			s_flags = "DE timing only";
		break;
	}
	if (flags & 0x80)
		add_str(s_flags, "interlaced");
	switch (flags & 0x61) {
	case 0x20:
		add_str(s_flags, "field sequential L/R");
		break;
	case 0x40:
		add_str(s_flags, "field sequential R/L");
		break;
	case 0x21:
		add_str(s_flags, "interleaved right even");
		break;
	case 0x41:
		add_str(s_flags, "interleaved left even");
		break;
	case 0x60:
		add_str(s_flags, "four way interleaved");
		break;
	case 0x61:
		add_str(s_flags, "side by side interleaved");
		break;
	default:
		break;
	}

	if (!ha || !hbl || !hso || !hspw || !va || !vbl || !vso || !vspw)
		fail("\n  0 values in the detailed timings:\n"
		     "    Horizontal Active/Blanking %u/%u\n"
		     "    Horizontal Sync Offset/Width %u/%u\n"
		     "    Vertical Active/Blanking %u/%u\n"
		     "    Vertical Sync Offset/Width %u/%u\n",
		     ha, hbl, hso, hspw, va, vbl, vso, vspw);

	pixclk_khz = (x[0] + (x[1] << 8)) * 10;
	if (pixclk_khz < 10000)
		fail("Pixelclock < 10 MHz\n");
	if ((ha + hbl) && (va + vbl))
		refresh = (pixclk_khz * 1000.0) / ((ha + hbl) * (va + vbl));
	else
		refresh = 0.0;
	hor_mm = x[12] + ((x[14] & 0xf0) << 4);
	vert_mm = x[13] + ((x[14] & 0x0f) << 8);
	printf("%sDetailed mode: Clock %.3f MHz, %u mm x %u mm\n"
	       "%s               %4u %4u %4u %4u (%3u %3u %3d) hborder %u\n"
	       "%s               %4u %4u %4u %4u (%3u %3u %3d) vborder %u\n"
	       "%s               %s%s\n"
	       "%s               VertFreq: %.3f Hz, HorFreq: %.3f kHz\n",
	       prefix,
	       pixclk_khz / 1000.0,
	       hor_mm, vert_mm,
	       prefix,
	       ha, ha + hso, ha + hso + hspw, ha + hbl, hso, hspw, hbl - hso - hspw, hborder,
	       prefix,
	       va, va + vso, va + vso + vspw, va + vbl, vso, vspw, vbl - vso - vspw, vborder,
	       prefix,
	       s_sync.c_str(), s_flags.c_str(),
	       prefix,
	       refresh, ha + hbl ? (double)pixclk_khz / (ha + hbl) : 0.0);
	if (hso + hspw >= hbl)
		fail("0 or negative horizontal back porch\n");
	if (vso + vspw >= vbl)
		fail("0 or negative vertical back porch\n");
	if ((!state.max_display_width_mm && hor_mm) ||
	    (!state.max_display_height_mm && vert_mm)) {
		fail("Mismatch of image size vs display size: image size is set, but not display size\n");
	} else if ((state.max_display_width_mm && !hor_mm) ||
		   (state.max_display_height_mm && !vert_mm)) {
		fail("Mismatch of image size vs display size: image size is not set, but display size is\n");
	} else if (!hor_mm && !vert_mm) {
		/* this is valid */
	} else if (hor_mm > state.max_display_width_mm + 9 ||
		   vert_mm > state.max_display_height_mm + 9) {
		fail("Mismatch of image size %ux%u mm vs display size %ux%u mm\n",
		     hor_mm, vert_mm, state.max_display_width_mm, state.max_display_height_mm);
	} else if (hor_mm < state.max_display_width_mm - 9 &&
		   vert_mm < state.max_display_height_mm - 9) {
		fail("Mismatch of image size %ux%u mm vs display size %ux%u mm\n",
		     hor_mm, vert_mm, state.max_display_width_mm, state.max_display_height_mm);
	}
	if (refresh) {
		state.min_vert_freq_hz = min(state.min_vert_freq_hz, refresh);
		state.max_vert_freq_hz = max(state.max_vert_freq_hz, refresh);
	}
	if (pixclk_khz && (ha + hbl)) {
		state.min_hor_freq_hz = min(state.min_hor_freq_hz, (pixclk_khz * 1000) / (ha + hbl));
		state.max_hor_freq_hz = max(state.max_hor_freq_hz, (pixclk_khz * 1000) / (ha + hbl));
		state.max_pixclk_khz = max(state.max_pixclk_khz, pixclk_khz);
	}
	if (state.has_spwg && state.timing_descr_cnt == 2)
		printf("SPWG Module Revision: %hhu\n", x[17]);
}

static void detailed_block(edid_state &state, const unsigned char *x)
{
	static const unsigned char zero_descr[18] = { 0 };
	static int seen_non_detailed_descriptor;
	unsigned cnt;
	unsigned i;

	state.timing_descr_cnt++;
	if (x[0] || x[1]) {
		detailed_timings(state, "", x);
		if (seen_non_detailed_descriptor)
			fail("Invalid detailed timing descriptor ordering\n");
		return;
	}

	state.cur_block = "Display Descriptor";
	/* Monitor descriptor block, not detailed timing descriptor. */
	if (x[2] != 0) {
		/* 1.3, 3.10.3 */
		fail("Monitor descriptor block has byte 2 nonzero (0x%02x)\n", x[2]);
	}
	if ((state.edid_minor < 4 || x[3] != 0xfd) && x[4] != 0x00) {
		/* 1.3, 3.10.3 */
		fail("Monitor descriptor block has byte 4 nonzero (0x%02x)\n", x[4]);
	}

	seen_non_detailed_descriptor = 1;
	if (state.edid_minor == 0)
		fail("Has descriptor blocks other than detailed timings\n");

	if (!memcmp(x, zero_descr, sizeof(zero_descr))) {
		state.cur_block = "Empty Descriptor";
		printf("%s\n", state.cur_block.c_str());
		fail("Use Dummy Descriptor instead of all zeroes\n");
	}

	switch (x[3]) {
	case 0x0e:
		detailed_epi(state, x);
		return;
	case 0x10:
		state.cur_block = "Dummy Descriptor";
		printf("%s\n", state.cur_block.c_str());
		for (i = 5; i < 18; i++) {
			if (x[i]) {
				fail("Dummy block filled with garbage\n");
				break;
			}
		}
		return;
	case 0xf7:
		state.cur_block = "Established timings III";
		printf("%s\n", state.cur_block.c_str());
		for (i = 0; i < 44; i++)
			if (x[6 + i / 8] & (1 << (7 - i % 8)))
				print_timings(state, "  ", find_dmt_id(established_timings3_dmt_ids[i]), "DMT");
		return;
	case 0xf8:
		state.cur_block = "CVT 3 Byte Timing Codes";
		printf("%s\n", state.cur_block.c_str());
		if (x[5] != 0x01) {
			fail("Invalid version number %u\n", x[5]);
			return;
		}
		for (i = 0; i < 4; i++)
			detailed_cvt_descriptor(state, x + 6 + (i * 3), (i == 0));
		return;
	case 0xf9:
		state.cur_block = "Display Color Management Data";
		printf("%s\n", state.cur_block.c_str());
		printf("  Version:  %d\n", x[5]);
		printf("  Red a3:   %.2f\n", (short)(x[6] | (x[7] << 8)) / 100.0);
		printf("  Red a2:   %.2f\n", (short)(x[8] | (x[9] << 8)) / 100.0);
		printf("  Green a3: %.2f\n", (short)(x[10] | (x[11] << 8)) / 100.0);
		printf("  Green a2: %.2f\n", (short)(x[12] | (x[13] << 8)) / 100.0);
		printf("  Blue a3:  %.2f\n", (short)(x[14] | (x[15] << 8)) / 100.0);
		printf("  Blue a2:  %.2f\n", (short)(x[16] | (x[17] << 8)) / 100.0);
		return;
	case 0xfa:
		state.cur_block = "Standard Timing Identifications";
		printf("%s\n", state.cur_block.c_str());
		for (cnt = i = 0; i < 6; i++) {
			if (x[5 + i * 2] != 0x01 || x[5 + i * 2 + 1] != 0x01)
				cnt++;
			print_standard_timing(state, x[5 + i * 2], x[5 + i * 2 + 1]);
		}
		if (!cnt)
			warn("%s block without any timings\n", state.cur_block.c_str());
		return;
	case 0xfb: {
		unsigned w_x, w_y;
		unsigned gamma;

		state.cur_block = "Color Point Data";
		printf("%s\n", state.cur_block.c_str());
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
			return;
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
		return;
	}
	case 0xfc:
		state.cur_block = "Display Product Name";
		state.has_name_descriptor = 1;
		printf("%s: %s\n", state.cur_block.c_str(), extract_string(x + 5, 13));
		return;
	case 0xfd:
		detailed_display_range_limits(state, x);
		return;
	case 0xfe:
		if (!state.has_spwg || state.timing_descr_cnt < 3) {
			state.cur_block = "Alphanumeric Data String";
			printf("%s: %s\n", state.cur_block.c_str(),
			       extract_string(x + 5, 13));
			return;
		}
		if (state.timing_descr_cnt == 3) {
			char buf[6] = { 0 };

			state.cur_block = "SPWG Descriptor #3";
			memcpy(buf, x + 5, 5);
			if (strlen(buf) != 5)
				fail("Invalid PC Maker P/N length\n");
			printf("SPWG PC Maker P/N: %s\n", buf);
			printf("SPWG LCD Supplier EEDID Revision: %hhu\n", x[10]);
			printf("SPWG Manufacturer P/N: %s\n", extract_string(x + 11, 7));
		} else {
			state.cur_block = "SPWG Descriptor #4";
			printf("SMBUS Values: 0x%02hhx 0x%02hhx 0x%02hhx 0x%02hhx"
			       " 0x%02hhx 0x%02hhx 0x%02hhx 0x%02hhx\n",
			       x[5], x[6], x[7], x[8], x[9], x[10], x[11], x[12]);
			printf("LVDS Channels: %hhu\n", x[13]);
			printf("Panel Self Test %sPresent\n", x[14] ? "" : "Not");
			if (x[15] != 0x0a || x[16] != 0x20 || x[17] != 0x20)
				fail("Invalid trailing data\n");
		}
		return;
	case 0xff:
		state.cur_block = "Display Product Serial Number";
		printf("%s: %s\n", state.cur_block.c_str(),
		       extract_string(x + 5, 13));
		state.has_serial_string = 1;
		return;
	default:
		printf("%s Description 0x%02hhx:",
		       x[3] <= 0x0f ? "Manufacturer-Specified" : "Unknown", x[3]);
		hex_block(" ", x + 2, 16);
		if (x[3] > 0x0f)
			warn("Unknown Description Type 0x%02hhx\n", x[3]);
		return;
	}
}

void parse_base_block(edid_state &state, const unsigned char *edid)
{
	time_t the_time;
	struct tm *ptm;
	int analog, i;
	unsigned col_x, col_y;
	int has_preferred_timing = 0;

	state.cur_block = "EDID Structure Version & Revision";
	printf("EDID version: %hhu.%hhu\n", edid[0x12], edid[0x13]);
	if (edid[0x12] == 1) {
		state.edid_minor = edid[0x13];
		if (state.edid_minor > 4)
			warn("Unknown EDID minor version %u, assuming 1.4 conformance\n", state.edid_minor);
		if (state.edid_minor < 3)
			warn("EDID 1.%u is deprecated, do not use\n", state.edid_minor);
	} else {
		fail("Unknown EDID major version\n");
	}

	state.cur_block = "Vendor & Product Identification";
	printf("Manufacturer: %s Model %u Serial Number %u\n",
	       manufacturer_name(edid + 0x08),
	       (unsigned short)(edid[0x0a] + (edid[0x0b] << 8)),
	       (unsigned)(edid[0x0c] + (edid[0x0d] << 8) +
			  (edid[0x0e] << 16) + (edid[0x0f] << 24)));
	state.has_serial_number = edid[0x0c] || edid[0x0d] || edid[0x0e] || edid[0x0f];
	/* XXX need manufacturer ID table */

	time(&the_time);
	ptm = localtime(&the_time);

	unsigned char week = edid[0x10];
	int year = 1990 + edid[0x11];

	if (week) {
		if (state.edid_minor <= 3 && week == 0xff)
			fail("EDID 1.3 does not support week 0xff\n");
		// The max week is 53 in EDID 1.3 and 54 in EDID 1.4.
		// No idea why there is a difference.
		if (state.edid_minor <= 3 && week == 54)
			fail("EDID 1.3 does not support week 54\n");
		if (week != 0xff && week > 54)
			fail("Invalid week %u of manufacture\n", week);
		if (week != 0xff)
			printf("Made in week %hhu of %d\n", week, year);
	}
	if (week == 0xff)
		printf("Model year %d\n", year);
	else if (!week)
		printf("Made in year %d\n", year);
	if (year - 1 > ptm->tm_year + 1900)
		fail("The year %d is more than one year in the future\n", year);

	/* display section */

	state.cur_block = "Basic Display Parameters & Features";
	if (edid[0x14] & 0x80) {
		analog = 0;
		printf("Digital display\n");
		if (state.edid_minor >= 4) {
			if ((edid[0x14] & 0x70) == 0x00)
				printf("Color depth is undefined\n");
			else if ((edid[0x14] & 0x70) == 0x70)
				fail("Color Bit Depth set to reserved value\n");
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
				   printf("Unknown (0x%02x) interface\n", edid[0x14] & 0x0f);
				   fail("Digital Video Interface Standard set to reserved value\n");
				   break;
			}
		} else if (state.edid_minor >= 2) {
			if (edid[0x14] & 0x01) {
				printf("DFP 1.x compatible TMDS\n");
			}
			if (edid[0x14] & 0x7e)
				fail("Byte 14 Digital Video Interface Standard set to reserved value\n");
		} else if (edid[0x14] & 0x7f) {
			fail("Digital Video Interface Standard set to reserved value\n");
		}
	} else {
		unsigned voltage = (edid[0x14] & 0x60) >> 5;
		unsigned sync = (edid[0x14] & 0x0f);

		analog = 1;
		printf("Analog display, Input voltage level: %s V\n",
		       voltage == 3 ? "0.7/0.7" :
		       voltage == 2 ? "1.0/0.4" :
		       voltage == 1 ? "0.714/0.286" :
		       "0.7/0.3");

		if (state.edid_minor >= 4) {
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
		state.max_display_width_mm = edid[0x15] * 10;
		state.max_display_height_mm = edid[0x16] * 10;
		if ((state.max_display_height_mm && !state.max_display_width_mm) ||
		    (state.max_display_width_mm && !state.max_display_height_mm))
			fail("Invalid maximum image size (%u cm x %u cm)\n",
			     state.max_display_width_mm, state.max_display_height_mm);
		else if (state.max_display_width_mm < 100 || state.max_display_height_mm < 100)
			warn("Dubious maximum image size (%ux%u is smaller than 10x10 cm)\n",
			     state.max_display_width_mm, state.max_display_height_mm);
	}
	else if (state.edid_minor >= 4 && (edid[0x15] || edid[0x16])) {
		if (edid[0x15])
			printf("Aspect ratio is %f (landscape)\n", 100.0/(edid[0x16] + 99));
		else
			printf("Aspect ratio is %f (portrait)\n", 100.0/(edid[0x15] + 99));
	} else {
		/* Either or both can be zero for 1.3 and before */
		printf("Image size is variable\n");
	}

	if (edid[0x17] == 0xff) {
		if (state.edid_minor >= 4)
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

	if (analog || state.edid_minor < 4) {
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
		if (state.edid_minor >= 4)
			printf("First detailed timing includes the native pixel format and preferred refresh rate\n");
		else
			printf("First detailed timing is preferred timing\n");
		has_preferred_timing = 1;
	} else if (state.edid_minor >= 4) {
		/* 1.4 always has a preferred timing and this bit means something else. */
		has_preferred_timing = 1;
	}

	if (edid[0x18] & 0x01) {
		if (state.edid_minor >= 4) {
			state.supports_continuous_freq = true;
			printf("Display is continuous frequency\n");
		} else {
			printf("Supports GTF timings within operating range\n");
			state.supports_gtf = true;
		}
	}

	state.cur_block = "Color Characteristics";
	printf("%s\n", state.cur_block.c_str());
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

	state.cur_block = "Established Timings I & II";
	if (edid[0x23] || edid[0x24] || edid[0x25]) {
		printf("%s\n", state.cur_block.c_str());
		for (i = 0; i < 17; i++) {
			if (edid[0x23 + i / 8] & (1 << (7 - i % 8))) {
				const struct timings *t;
				const char *suffix = "DMT";

				if (established_timings12[i].dmt_id) {
					t = find_dmt_id(established_timings12[i].dmt_id);
				} else {
					t = &established_timings12[i].t;
					suffix = established_timings12[i].std_name;
				}
				print_timings(state, "  ", t, suffix);
			}
		}
	} else {
		printf("%s: none\n", state.cur_block.c_str());
	}
	state.has_640x480p60_est_timing = edid[0x23] & 0x20;

	state.cur_block = "Standard Timings";
	bool found = false;
	for (i = 0; i < 8; i++) {
		if (edid[0x26 + i * 2] != 0x01 || edid[0x26 + i * 2 + 1] != 0x01) {
			found = true;
		}
	}
	if (found) {
		printf("%s\n", state.cur_block.c_str());
		for (i = 0; i < 8; i++)
			print_standard_timing(state, edid[0x26 + i * 2], edid[0x26 + i * 2 + 1]);
	} else {
		printf("%s: none\n", state.cur_block.c_str());
	}

	/* 18 byte descriptors */
	if (has_preferred_timing && !edid[0x36] && !edid[0x37])
		fail("Missing preferred timing\n");

	/* Look for SPWG Noteboook Panel EDID data blocks */
	if ((edid[0x36] || edid[0x37]) &&
	    (edid[0x48] || edid[0x49]) &&
	    !edid[0x5a] && !edid[0x5b] && edid[0x5d] == 0xfe &&
	    !edid[0x6c] && !edid[0x6d] && edid[0x6f] == 0xfe &&
	    (edid[0x79] == 1 || edid[0x79] == 2) && edid[0x7a] <= 1)
		state.has_spwg = true;

	detailed_block(state, edid + 0x36);
	detailed_block(state, edid + 0x48);
	detailed_block(state, edid + 0x5a);
	detailed_block(state, edid + 0x6c);
	state.has_spwg = false;

	if (edid[0x7e])
		printf("Has %u extension block%s\n", edid[0x7e], edid[0x7e] > 1 ? "s" : "");

	state.cur_block = "Base Block";
	do_checksum("", edid, EDID_PAGE_SIZE);
	if (state.edid_minor >= 3) {
		if (!state.has_name_descriptor)
			fail("Missing Display Product Name\n");
		if ((state.edid_minor == 3 || state.supports_continuous_freq) &&
		    !state.has_display_range_descriptor)
			fail("Missing Display Range Limits Descriptor\n");
	}
}
