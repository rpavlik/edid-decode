// SPDX-License-Identifier: MIT
/*
 * Copyright 2019 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 *
 * Author: Hans Verkuil <hverkuil-cisco@xs4all.nl>
 */

#include "edid-decode.h"

void edid_state::parse_vtb_ext_block(const unsigned char *x)
{
	printf("%s Version %u\n", block.c_str(), x[1]);
	if (x[1] != 1)
		fail("Invalid version %u\n", x[1]);

	unsigned num_dtb = x[2];
	unsigned num_cvt = x[3];
	unsigned num_st = x[4];

	x += 5;
	for (unsigned i = 0; i < num_dtb; i++, x += 18)
		detailed_timings("  ", x);
	for (unsigned i = 0; i < num_cvt; i++, x += 3)
		detailed_cvt_descriptor("  ", x, false);
	for (unsigned i = 0; i < num_st; i++, x += 2) {
		if ((x[1] & 0x3f) >= 60)
			print_standard_timing("  ", x[0], x[1] - 60, true);
		else
			print_standard_timing("  ", x[0], x[1], true, 0);
	}
}
