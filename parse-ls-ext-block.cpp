// SPDX-License-Identifier: MIT
/*
 * Copyright 2019 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 *
 * Author: Hans Verkuil <hverkuil-cisco@xs4all.nl>
 */

#include "edid-decode.h"

static void parse_string(const char *name, const unsigned char *x)
{
	if (!*x)
		return;
	printf("  %s: ", name);
	hex_block("", x + 1, *x, true, *x);
}

static void parse_string_table(const unsigned char *x)
{
	printf("  UTF Type: ");
	switch (x[0] & 7) {
	case 0: printf("UTF 8\n"); break;
	case 1: printf("UTF 16BE\n"); break;
	case 2: printf("UTF 32BE\n"); break;
	default:
		printf("Unknown (0x%02x)\n", x[0] & 7);
		fail("Unknown UTF Type (0x%02x)\n", x[0] & 7);
		break;
	}
	printf("  Country Code ID (ISO 3166-3): %u\n", ((x[1] & 0x3f) << 8) | x[2]);

	if (x[3] || x[4]) {
		char name[4];

		name[0] = ((x[3] & 0x7c) >> 2) + '@';
		name[1] = ((x[3] & 0x03) << 3) + ((x[4] & 0xe0) >> 5) + '@';
		name[2] = (x[4] & 0x1f) + '@';
		name[3] = 0;
		if (name[0] == '@') name[0] = ' ';
		if (name[1] == '@') name[1] = ' ';
		if (name[2] == '@') name[2] = ' ';
		printf("  Language ID: %s\n", name);
	}
	x += 5;
	parse_string("Manufacturer Name", x);
	x += x[0] + 1;
	parse_string("Model Name", x);
	x += x[0] + 1;
	parse_string("Serial Number", x);
}

void parse_ls_ext_block(edid_state &state, const unsigned char *x)
{
	const unsigned char *orig = x;

	printf("%s Version %u.%u Unicode Version %u.%u.%u\n",
	       state.cur_block.c_str(), x[1], x[2], (x[3] >> 4), x[3] & 0x0f, x[4]);
	x += 5;

	while (x[0] && x + x[0] < orig + 127) {
		parse_string_table(x + 1);
		x += x[0];
	}
}
