// SPDX-License-Identifier: MIT
/*
 * Copyright 2006-2012 Red Hat, Inc.
 * Copyright 2018-2021 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 *
 * Author: Adam Jackson <ajax@nwnk.net>
 * Maintainer: Hans Verkuil <hverkuil-cisco@xs4all.nl>
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include "edid-decode.h"

#define CELL_GRAN 8.0
#define MARGIN_PERC 1.8
#define GTF_MIN_PORCH 1.0
#define GTF_V_SYNC_RQD 3.0
#define GTF_H_SYNC_PERC 8.0
#define GTF_MIN_VSYNC_BP 550.0

timings edid_state::calc_gtf_mode(unsigned h_pixels, unsigned v_lines,
				  double ip_freq_rqd, bool int_rqd,
				  enum gtf_ip_parm ip_parm, bool margins_rqd,
				  bool secondary, double C, double M, double K, double J)
{
	timings t = {};
	/* C' and M' are part of the Blanking Duty Cycle computation */
	double C_PRIME = ((C - J) * K / 256.0) + J;
	double M_PRIME = K / 256.0 * M;

	double h_pixels_rnd = round(h_pixels / CELL_GRAN) * CELL_GRAN;
	double v_lines_rnd = int_rqd ? round(v_lines / 2.0) : v_lines;
	unsigned hor_margin = margins_rqd ?
		round(h_pixels_rnd * MARGIN_PERC / 100.0 / CELL_GRAN)  * CELL_GRAN : 0;
	unsigned vert_margin = margins_rqd ? round(MARGIN_PERC / 100.0 * v_lines_rnd) : 0;
	double interlace = int_rqd ? 0.5 : 0;
	double total_active_pixels = h_pixels_rnd + hor_margin * 2;

	t.hact = h_pixels_rnd;
	t.vact = v_lines;
	t.interlaced = int_rqd;

	double pixel_freq;
	double h_blank_pixels;
	double total_pixels;
	double v_sync_bp;

	if (ip_parm == gtf_ip_vert_freq) {
		// vertical frame frequency (Hz)
		double v_field_rate_rqd = int_rqd ? ip_freq_rqd * 2 : ip_freq_rqd;
		double h_period_est = ((1.0 / v_field_rate_rqd) - GTF_MIN_VSYNC_BP / 1000000.0) /
			(v_lines_rnd + vert_margin * 2 + GTF_MIN_PORCH + interlace) * 1000000.0;
		v_sync_bp = round(GTF_MIN_VSYNC_BP / h_period_est);
		double total_v_lines = v_lines_rnd + vert_margin * 2 +
			v_sync_bp + interlace + GTF_MIN_PORCH;
		double v_field_rate_est = 1.0 / h_period_est / total_v_lines * 1000000.0;
		double h_period = h_period_est / (v_field_rate_rqd / v_field_rate_est);
		double ideal_duty_cycle = C_PRIME - (M_PRIME * h_period / 1000.0);
		h_blank_pixels = round(total_active_pixels * ideal_duty_cycle /
				       (100.0 - ideal_duty_cycle) /
				       (2 * CELL_GRAN)) * 2 * CELL_GRAN;
		total_pixels = total_active_pixels + h_blank_pixels;
		pixel_freq = total_pixels / h_period;
	} else if (ip_parm == gtf_ip_hor_freq) {
		// horizontal frequency (kHz)
		double h_freq = ip_freq_rqd;
		v_sync_bp = round(GTF_MIN_VSYNC_BP * h_freq / 1000.0);
		double ideal_duty_cycle = C_PRIME - (M_PRIME / h_freq);
		h_blank_pixels = round(total_active_pixels * ideal_duty_cycle /
				       (100.0 - ideal_duty_cycle) /
				       (2 * CELL_GRAN)) * 2 * CELL_GRAN;
		total_pixels = total_active_pixels + h_blank_pixels;
		pixel_freq = total_pixels * h_freq / 1000.0;
	} else {
		// pixel clock rate (MHz)
		pixel_freq = ip_freq_rqd;
		double ideal_h_period =
			((C_PRIME - 100.0) +
			 sqrt(((100.0 - C_PRIME) * (100.0 - C_PRIME) +
			       (0.4 * M_PRIME * (total_active_pixels + hor_margin * 2) /
				pixel_freq)))) / 2.0 / M_PRIME * 1000.0;
		double ideal_duty_cycle = C_PRIME - (M_PRIME * ideal_h_period) / 1000.0;
		h_blank_pixels = round(total_active_pixels * ideal_duty_cycle /
				       (100.0 - ideal_duty_cycle) /
				       (2 * CELL_GRAN)) * 2 * CELL_GRAN;
		total_pixels = total_active_pixels + h_blank_pixels;
		double h_freq = pixel_freq / total_pixels * 1000.0;
		v_sync_bp = round(GTF_MIN_VSYNC_BP * h_freq / 1000.0);
	}

	double v_back_porch = v_sync_bp - GTF_V_SYNC_RQD;

	t.vbp = v_back_porch;
	t.vsync = GTF_V_SYNC_RQD;
	t.vfp = GTF_MIN_PORCH;
	t.pixclk_khz = round(1000.0 * pixel_freq);
	t.hsync = round(GTF_H_SYNC_PERC / 100.0 * total_pixels / CELL_GRAN) * CELL_GRAN;
	t.hfp = (h_blank_pixels / 2.0) - t.hsync;
	t.hbp = t.hfp + t.hsync;
	t.hborder = hor_margin;
	t.vborder = vert_margin;
	t.pos_pol_hsync = secondary;
	t.pos_pol_vsync = !secondary;
	t.rb = secondary ? RB_GTF : 0;
	return t;
}

void edid_state::edid_gtf_mode(unsigned refresh, struct timings &t)
{
	unsigned hratio = t.hratio;
	unsigned vratio = t.vratio;
	t = calc_gtf_mode(t.hact, t.vact, refresh, t.interlaced);
	t.hratio = hratio;
	t.vratio = vratio;
}

#define CVT_MIN_VSYNC_BP 550.0
#define CVT_MIN_V_PORCH 3
#define CVT_MIN_V_BPORCH 6
#define CVT_C_PRIME 30.0
#define CVT_M_PRIME 300.0
#define CVT_RB_MIN_VBLANK 460.0

// If rb == RB_CVT_V2, then alt means video-optimized (i.e. 59.94 instead of 60 Hz, etc.).
// If rb == RB_CVT_V3, then alt means that rb_h_blank is 160 instead of 80.
// Note: for RB_CVT_V3 this calculation is slightly different, but
// since CVT 1.3 is not yet public, I cannot update the calculation yet. For now
// it will follow V2. So RBv3 timings will be off for now.
timings edid_state::calc_cvt_mode(unsigned h_pixels, unsigned v_lines,
				  double ip_freq_rqd, unsigned rb, bool int_rqd,
				  bool margins_rqd, bool alt)
{
	timings t = {};

	t.hact = h_pixels;
	t.vact = v_lines;
	t.interlaced = int_rqd;

	double cell_gran = rb == RB_CVT_V2 ? 1 : CELL_GRAN;
	double h_pixels_rnd = floor(h_pixels / cell_gran) * cell_gran;
	double v_lines_rnd = int_rqd ? floor(v_lines / 2.0) : v_lines;
	unsigned hor_margin = margins_rqd ?
		floor((h_pixels_rnd * MARGIN_PERC / 100.0) / cell_gran) * cell_gran : 0;
	unsigned vert_margin = margins_rqd ? floor(MARGIN_PERC / 100.0 * v_lines_rnd) : 0;
	double interlace = int_rqd ? 0.5 : 0;
	double total_active_pixels = h_pixels_rnd + hor_margin * 2;
	double v_field_rate_rqd = int_rqd ? ip_freq_rqd * 2 : ip_freq_rqd;
	double clock_step = rb == RB_CVT_V2 ? 0.001 : 0.25;
	double h_blank = (rb == RB_CVT_V1 || (rb == RB_CVT_V3 && alt)) ? 160 : 80;
	double rb_v_fporch = rb == RB_CVT_V1 ? 3 : 1;
	double refresh_multiplier = (rb == RB_CVT_V2 && alt) ? 1000.0 / 1001.0 : 1;
	double h_sync = 32;

	double v_sync;
	double pixel_freq;
	double v_blank;
	double v_sync_bp;

	/* Determine VSync Width from aspect ratio */
	if ((t.vact * 4 / 3) == t.hact)
		v_sync = 4;
	else if ((t.vact * 16 / 9) == t.hact)
		v_sync = 5;
	else if ((t.vact * 16 / 10) == t.hact)
		v_sync = 6;
	else if (!(t.vact % 4) && ((t.vact * 5 / 4) == t.hact))
		v_sync = 7;
	else if ((t.vact * 15 / 9) == t.hact)
		v_sync = 7;
	else                        /* Custom */
		v_sync = 10;

	if (rb >= RB_CVT_V2)
		v_sync = 8;

	if (rb == 0) {
		double h_period_est = ((1.0 / v_field_rate_rqd) - CVT_MIN_VSYNC_BP / 1000000.0) /
			(v_lines_rnd + vert_margin * 2 + CVT_MIN_V_PORCH + interlace) * 1000000.0;
		v_sync_bp = floor(CVT_MIN_VSYNC_BP / h_period_est) + 1;
		if (v_sync_bp < v_sync + CVT_MIN_V_BPORCH)
			v_sync_bp = v_sync + CVT_MIN_V_BPORCH;
		v_blank = v_sync_bp + CVT_MIN_V_PORCH;
		double ideal_duty_cycle = CVT_C_PRIME - (CVT_M_PRIME * h_period_est / 1000.0);
		if (ideal_duty_cycle < 20)
			ideal_duty_cycle = 20;
		h_blank = floor(total_active_pixels * ideal_duty_cycle /
				(100.0 - ideal_duty_cycle) /
				(2 * CELL_GRAN)) * 2 * CELL_GRAN;
		double total_pixels = total_active_pixels + h_blank;
		h_sync = floor(total_pixels * 0.08 / CELL_GRAN) * CELL_GRAN;
		pixel_freq = floor((total_pixels / h_period_est) / clock_step) * clock_step;
	} else {
		double h_period_est = ((1000000.0 / v_field_rate_rqd) - CVT_RB_MIN_VBLANK) /
					(v_lines_rnd + vert_margin * 2);
		double vbi_lines = floor(CVT_RB_MIN_VBLANK / h_period_est) + 1;
		double rb_min_vbi = rb_v_fporch + v_sync + CVT_MIN_V_BPORCH;
		v_blank = vbi_lines < rb_min_vbi ? rb_min_vbi : vbi_lines;
		double total_v_lines = v_blank + v_lines_rnd + vert_margin * 2 + interlace;
		if (rb == RB_CVT_V1)
			v_sync_bp = v_blank - rb_v_fporch;
		else
			v_sync_bp = v_sync + CVT_MIN_V_BPORCH;
		double total_pixels = h_blank + total_active_pixels;
		pixel_freq = floor((v_field_rate_rqd * total_v_lines * total_pixels / 1000000.0 *
				    refresh_multiplier) / clock_step) * clock_step;
	}

	t.vbp = v_sync_bp - v_sync;
	t.vsync = v_sync;
	t.vfp = v_blank - t.vbp - t.vsync;
	t.pixclk_khz = round(1000.0 * pixel_freq);
	t.hsync = h_sync;
	t.hfp = (h_blank / 2.0) - t.hsync;
	t.hbp = t.hfp + t.hsync;
	t.hborder = hor_margin;
	t.vborder = vert_margin;
	t.rb = rb;
	if (alt && (rb == RB_CVT_V2 || rb == RB_CVT_V3))
		t.rb |= RB_FLAG;
	t.pos_pol_hsync = t.rb;
	t.pos_pol_vsync = !t.rb;
	calc_ratio(&t);
	return t;
}

void edid_state::edid_cvt_mode(unsigned refresh, struct timings &t)
{
	unsigned hratio = t.hratio;
	unsigned vratio = t.vratio;

	t = calc_cvt_mode(t.hact, t.vact, refresh, t.rb & ~RB_FLAG, t.interlaced,
			  false, t.rb & RB_FLAG);
	t.hratio = hratio;
	t.vratio = vratio;
}
