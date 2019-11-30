// SPDX-License-Identifier: MIT
/*
 * Copyright 2019 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 *
 * Author: Hans Verkuil <hverkuil-cisco@xs4all.nl>
 */

#include "edid-decode.h"

static void parse_digital_interface(edid_state &state, const unsigned char *x)
{
	state.cur_block = "Digital Interface";
	printf("%s\n", state.cur_block.c_str());

	printf("  Supported Digital Interface: ");
	unsigned short v = x[2];
	switch (v) {
	case 0x00: printf("Analog Video Input\n"); return;
	case 0x01: printf("DVI\n"); break;
	case 0x02: printf("DVI Single Link\n"); break;
	case 0x03: printf("DVI Dual Link - High Resolution\n"); break;
	case 0x04: printf("DVI Dual Link - High Color\n"); break;
	case 0x05: printf("DVI - Consumer Electronics\n"); break;
	case 0x06: printf("Plug & Display\n"); break;
	case 0x07: printf("DFP\n"); break;
	case 0x08: printf("Open LDI - Single Link\n"); break;
	case 0x09: printf("Open LDI - Dual Link\n"); break;
	case 0x0a: printf("Open LDI - Consumer Electronics\n"); break;
	default:
		   printf("Unknown (0x%02x)\n", v);
		   fail("Unknown Digital Interface 0x%02x\n", v);
		   break;
	}

	switch ((x[3]) >> 6) {
	case 0x00:
		if (x[3] || x[4] || x[5] || x[6])
			fail("Bytes 3-6 should be 0\n");
		break;
	case 0x01:
		printf("  Version %u.%u Release %u.%u\n", x[3] & 0x3f, x[4], x[5], x[6]);
		if (x[4] > 99)
			fail("Version number > 99\n");
		if (x[6] > 99)
			fail("Release number > 99\n");
		break;
	case 0x02:
		if (x[3] & 0x3f)
			fail("Bits 5-0 of byte 3 should be 0\n");
		if (x[5] || x[6])
			fail("Bytes 5-6 should be 0\n");
		printf("  Letter Designation: %c\n", x[4]);
		break;
	case 0x03:
		if (x[3] & 0x3f)
			fail("Bits 5-0 of byte 3 should be 0\n");
		printf("  Date Code: Year %u Week %u Day %u\n", 1990 + x[4], x[5], x[6]);
		if (!x[5] || x[5] > 12)
			fail("Bad month number\n");
		if (!x[6] || x[6] > 31)
			fail("Bad day number\n");
		break;
	}

	v = x[7];
	printf("  Data Enable Signal Usage %sAvailable\n",
	       (v & 0x80) ? "" : "Not ");
	if (v & 0x80)
		printf("  Data Enable Signal %s\n",
		       (v & 0x40) ? "Low" : "High");
	else if (v & 0x40)
		fail("Bit 6 of byte 7 should be 0\n");
	printf("  Edge of Shift Clock: ");
	switch ((v >> 4) & 0x03) {
	case 0: printf("Not specified\n"); break;
	case 1: printf("Use rising edge of shift clock\n"); break;
	case 2: printf("Use falling edge of shift clock\n"); break;
	case 3: printf("Use both edges of shift clock\n"); break;
	}
	printf("  HDCP is %ssupported\n", (v & 0x08) ? "" : "not ");
	printf("  Digital Receivers do %ssupport Double Clocking of Input Data\n",
	       (v & 0x04) ? "" : "not ");
	printf("  Packetized Digital Video is %ssupported\n", (v & 0x02) ? "" : "not ");
	if (v & 0x01)
		fail("Bit 0 of byte 7 should be 0\n");

	v = x[8];
	printf("  Data Formats: ");
	switch (v) {
	case 0x15: printf("8-Bit Over 8-Bit RGB\n"); break;
	case 0x19: printf("12-Bit Over 12-Bit RGB\n"); break;
	case 0x24: printf("24-Bit MSB-Aligned RGB (Single Link)\n"); break;
	case 0x48: printf("48-Bit MSB-Aligned RGB (Dual Link - High Resolution)\n"); break;
	case 0x49: printf("48-Bit MSB-Aligned RGB (Dual Link - High Color)\n"); break;
	default:
		   printf("Unknown (0x%02x)\n", v);
		   fail("Unknown Data Format 0x%02x\n", v);
		   break;
	}
	if (x[2] == 0x03 && v != 0x48)
		fail("Data Format should be 0x48, not 0x%02x\n", v);
	if (x[2] == 0x04 && v != 0x49)
		fail("Data Format should be 0x49, not 0x%02x\n", v);

	v = x[9];
	printf("  Minimum Pixel Clock Frequency Per Link: %u MHz\n", v);
	if (v == 0 || v == 0xff)
		fail("Invalid Min-PCF 0x%02x\n", v);

	v = x[10] | (x[11] << 8);
	printf("  Maximum Pixel Clock Frequency Per Link: %u MHz\n", v);
	if (v == 0 || v == 0xffff)
		fail("Invalid Max-PCF 0x%04x\n", v);

	v = x[12] | (x[13] << 8);
	printf("  Crossover Frequency: %u MHz\n", v);
	if (v == 0 || v == 0xffff)
		fail("Invalid Crossover Frequence 0x%04x\n", v);
}

static void parse_display_device(edid_state &state, const unsigned char *x)
{
	state.cur_block = "Display Device";
	printf("%s\n", state.cur_block.c_str());

	printf("  Sub-Pixel Layout: ");
	unsigned char v = x[0x0e];
	switch (v) {
	case 0x00: printf("Not defined\n"); break;
	case 0x01: printf("RGB\n"); break;
	case 0x02: printf("BGR\n"); break;
	case 0x03: printf("Quad Pixel - G at bottom left & top right\n"); break;
	case 0x04: printf("Quad Pixel - G at bottom right & top left\n"); break;
	default:
		   printf("Unknown (0x%02x)\n", v);
		   fail("Unknown Sub-Pixel Layout 0x%02x\n", v);
		   break;
	}
	printf("  Sub-Pixel Configuration: ");
	v = x[0x0f];
	switch (v) {
	case 0x00: printf("Not defined\n"); break;
	case 0x01: printf("Delta (Tri-ad)\n"); break;
	case 0x02: printf("Stripe\n"); break;
	case 0x03: printf("Stripe Offset\n"); break;
	case 0x04: printf("Quad Pixel\n"); break;
	default:
		   printf("Unknown (0x%02x)\n", v);
		   fail("Unknown Sub-Pixel Configuration 0x%02x\n", v);
		   break;
	}
	printf("  Sub-Pixel Shape: ");
	v = x[0x10];
	switch (v) {
	case 0x00: printf("Not defined\n"); break;
	case 0x01: printf("Round\n"); break;
	case 0x02: printf("Square\n"); break;
	case 0x03: printf("Rectangular\n"); break;
	case 0x04: printf("Oval\n"); break;
	case 0x05: printf("Elliptical\n"); break;
	default:
		   printf("Unknown (0x%02x)\n", v);
		   fail("Unknown Sub-Pixel Shape 0x%02x\n", v);
		   break;
	}
	if (x[0x11])
		printf("  Horizontal Dot/Pixel Pitch: %.2f mm\n",
		       x[0x11] / 100.0);
	if (x[0x12])
		printf("  Vertical Dot/Pixel Pitch: %.2f mm\n",
		       x[0x12] / 100.0);
	v = x[0x13];
	printf("  Display Device %s a Fixed Pixel Format\n",
	       (v & 0x80) ? "has" : "does not have");
	printf("  View Direction: ");
	switch ((v & 0x60) >> 5) {
	case 0x00: printf("Not specified\n"); break;
	case 0x01: printf("Direct\n"); break;
	case 0x02: printf("Reflected\n"); break;
	case 0x03: printf("Direct & Reflected\n"); break;
	}
	printf("  Display Device uses %stransparent background\n",
	       (v & 0x10) ? "" : "non-");
	printf("  Physical Implementation: ");
	switch ((v & 0x0c) >> 2) {
	case 0x00: printf("Not specified\n"); break;
	case 0x01: printf("Large Image device for group viewing\n"); break;
	case 0x02: printf("Desktop or personal display\n"); break;
	case 0x03: printf("Eyepiece type personal display\n"); break;
	}
	printf("  Monitor/display does %ssupport DDC/CI\n",
	       (v & 0x02) ? "" : "not ");
	if (v & 0x01)
		fail("Bit 0 of byte 0x13 should be 0\n");
}

static void parse_display_caps(edid_state &state, const unsigned char *x)
{
	state.cur_block = "Display Capabities & Feature Support Set";
	printf("%s\n", state.cur_block.c_str());
}

static void parse_display_xfer(edid_state &state, const unsigned char *x)
{
	state.cur_block = "Display Transfer Characteristics - Gamma";
	printf("%s\n", state.cur_block.c_str());
}

void parse_di_ext_block(edid_state &state, const unsigned char *x)
{
	printf("%s Version %u\n", state.cur_block.c_str(), x[1]);
	if (!x[1])
		fail("Invalid version 0\n");

	parse_digital_interface(state, x);
	parse_display_device(state, x);
	parse_display_caps(state, x);
	parse_display_xfer(state, x);
}
