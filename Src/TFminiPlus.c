/**
 * \file    TFminiPlus.c
 * \brief   TFmini Plus lidar frame parser and configuration helpers.
 *
 * AUTHOR  Roman Garanin
 */

#include "TFminiPlus.h"

#include <string.h>

#define TFMINI_HEADER_BYTE 0x59u
#define TFMINI_CMD_HEADER  0x5Au

static uint8_t frame_checksum(const uint8_t *frame)
{
	uint8_t sum = 0;
	for (uint8_t i = 0; i < 8u; i++) {
		sum = (uint8_t)(sum + frame[i]);
	}
	return sum;
}

static uint16_t read_u16_le(const uint8_t *p)
{
	return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static void write_u32_le(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v & 0xFFu);
	p[1] = (uint8_t)((v >> 8) & 0xFFu);
	p[2] = (uint8_t)((v >> 16) & 0xFFu);
	p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static int16_t temp_raw_to_c(uint16_t raw)
{
	/* Product manual: Temp(°C) = Temp_raw / 8 - 256 */
	return (int16_t)((int32_t)raw / 8 - 256);
}

static void ring_reset(tfmini_plus_t *ctx)
{
	ctx->_.head = 0;
	ctx->_.tail = 0;
	ctx->_.count = 0;
}

static size_t ring_free(const tfmini_plus_t *ctx)
{
	return TFMINI_RX_BUFFER_SIZE - ctx->_.count;
}

static bool ring_push(tfmini_plus_t *ctx, const uint8_t *data, size_t len)
{
	size_t first;

	if (len > ring_free(ctx)) {
		return false;
	}
	if (len == 0) {
		return true;
	}

	first = TFMINI_RX_BUFFER_SIZE - ctx->_.tail;
	if (first > len) {
		first = len;
	}
	memcpy(&ctx->_.rx[ctx->_.tail], data, first);
	if (len > first) {
		memcpy(&ctx->_.rx[0], data + first, len - first);
	}
	ctx->_.tail = (ctx->_.tail + len) % TFMINI_RX_BUFFER_SIZE;
	ctx->_.count += len;
	return true;
}

static uint8_t ring_at(const tfmini_plus_t *ctx, size_t offset)
{
	return ctx->_.rx[(ctx->_.head + offset) % TFMINI_RX_BUFFER_SIZE];
}

static void ring_discard(tfmini_plus_t *ctx, size_t n)
{
	if (n > ctx->_.count) {
		n = ctx->_.count;
	}
	ctx->_.head = (ctx->_.head + n) % TFMINI_RX_BUFFER_SIZE;
	ctx->_.count -= n;
}

static void ring_copy_frame(const tfmini_plus_t *ctx, uint8_t out[TFMINI_FRAME_LEN])
{
	for (size_t i = 0; i < TFMINI_FRAME_LEN; i++) {
		out[i] = ring_at(ctx, i);
	}
}

/**
 * Locate next 0x59 0x59 starting at offset 0 of the ring.
 * Discards leading noise. Returns true if a header is at head.
 */
static bool sync_to_header(tfmini_plus_t *ctx)
{
	while (ctx->_.count >= 2u) {
		if (ring_at(ctx, 0) == TFMINI_HEADER_BYTE &&
		    ring_at(ctx, 1) == TFMINI_HEADER_BYTE) {
			return true;
		}
		ring_discard(ctx, 1);
	}
	return false;
}

static bool try_parse_one_frame(tfmini_plus_t *ctx, bool *checksum_failed)
{
	uint8_t frame[TFMINI_FRAME_LEN];

	*checksum_failed = false;

	if (!sync_to_header(ctx)) {
		return false;
	}
	if (ctx->_.count < TFMINI_FRAME_LEN) {
		return false;
	}

	ring_copy_frame(ctx, frame);

	if (frame_checksum(frame) != frame[8]) {
		*checksum_failed = true;
		ctx->frames_bad_checksum++;
		/* Skip first header byte and resync on next opportunity. */
		ring_discard(ctx, 1);
		return false;
	}

	ctx->distance = read_u16_le(&frame[2]);
	ctx->strength = read_u16_le(&frame[4]);
	ctx->temperature_c = temp_raw_to_c(read_u16_le(&frame[6]));
	ctx->has_data = true;
	ctx->frames_ok++;

	ring_discard(ctx, TFMINI_FRAME_LEN);
	return true;
}

static tfmini_status_t parse_available(tfmini_plus_t *ctx)
{
	bool saw_ok = false;
	bool saw_bad_cs = false;

	for (;;) {
		bool checksum_failed = false;
		if (try_parse_one_frame(ctx, &checksum_failed)) {
			saw_ok = true;
			continue;
		}
		if (checksum_failed) {
			saw_bad_cs = true;
			continue;
		}
		break;
	}

	if (saw_ok) {
		return TFMINI_OK;
	}
	if (saw_bad_cs) {
		return TFMINI_BAD_CHECKSUM;
	}
	return TFMINI_NEED_MORE;
}

void tfmini_plus_init(tfmini_plus_t *ctx)
{
	if (ctx == NULL) {
		return;
	}
	memset(ctx, 0, sizeof(*ctx));
}

tfmini_status_t tfmini_plus_feed(tfmini_plus_t *ctx, const uint8_t *data, size_t len)
{
	bool overflowed = false;
	tfmini_status_t st;

	if (ctx == NULL || data == NULL) {
		return TFMINI_INVALID_ARG;
	}
	if (len == 0) {
		return TFMINI_NEED_MORE;
	}

	if (!ring_push(ctx, data, len)) {
		ctx->overflows++;
		overflowed = true;
		ring_reset(ctx);
		if (len > TFMINI_RX_BUFFER_SIZE || !ring_push(ctx, data, len)) {
			ring_reset(ctx);
			return TFMINI_OVERFLOW;
		}
	}

	st = parse_available(ctx);
	if (overflowed) {
		return TFMINI_OVERFLOW;
	}
	return st;
}

bool tfmini_plus_distance_valid(const tfmini_plus_t *ctx)
{
	if (ctx == NULL || !ctx->has_data) {
		return false;
	}
	return ctx->distance != 0u && ctx->distance != TFMINI_DIST_INVALID;
}

bool tfmini_plus_strength_valid(const tfmini_plus_t *ctx)
{
	if (ctx == NULL || !ctx->has_data) {
		return false;
	}
	return ctx->strength >= TFMINI_STRENGTH_MIN &&
	       ctx->strength <= TFMINI_STRENGTH_MAX;
}

bool tfmini_plus_measurement_valid(const tfmini_plus_t *ctx)
{
	return tfmini_plus_distance_valid(ctx) && tfmini_plus_strength_valid(ctx);
}

const char *tfmini_status_str(tfmini_status_t status)
{
	switch (status) {
	case TFMINI_OK:
		return "OK";
	case TFMINI_NEED_MORE:
		return "NEED_MORE";
	case TFMINI_BAD_CHECKSUM:
		return "BAD_CHECKSUM";
	case TFMINI_OVERFLOW:
		return "OVERFLOW";
	case TFMINI_INVALID_ARG:
		return "INVALID_ARG";
	default:
		return "UNKNOWN";
	}
}

/* ---- Configuration command builders ---- */

static size_t cmd_write(uint8_t *out, size_t out_cap, const uint8_t *payload, size_t payload_len)
{
	size_t total;
	uint8_t sum;
	size_t i;

	if (out == NULL || payload == NULL || payload_len == 0) {
		return 0;
	}

	total = payload_len + 1u; /* + checksum */
	if (total > out_cap || total > TFMINI_CMD_MAX_LEN) {
		return 0;
	}

	memcpy(out, payload, payload_len);
	sum = 0;
	for (i = 0; i < payload_len; i++) {
		sum = (uint8_t)(sum + out[i]);
	}
	out[payload_len] = sum;
	return total;
}

size_t tfmini_cmd_get_version(uint8_t *out, size_t out_cap)
{
	const uint8_t p[] = { TFMINI_CMD_HEADER, 0x04, 0x01 };
	return cmd_write(out, out_cap, p, sizeof(p));
}

size_t tfmini_cmd_system_reset(uint8_t *out, size_t out_cap)
{
	const uint8_t p[] = { TFMINI_CMD_HEADER, 0x04, 0x02 };
	return cmd_write(out, out_cap, p, sizeof(p));
}

size_t tfmini_cmd_set_frame_rate(uint8_t *out, size_t out_cap, uint16_t hz)
{
	uint8_t p[5];

	if (hz < TFMINI_FRAME_RATE_MIN || hz > TFMINI_FRAME_RATE_MAX) {
		return 0;
	}

	p[0] = TFMINI_CMD_HEADER;
	p[1] = 0x06;
	p[2] = 0x03;
	p[3] = (uint8_t)(hz & 0xFFu);
	p[4] = (uint8_t)((hz >> 8) & 0xFFu);
	return cmd_write(out, out_cap, p, sizeof(p));
}

size_t tfmini_cmd_set_baud_rate(uint8_t *out, size_t out_cap, tfmini_baud_t baud)
{
	uint8_t p[7];

	switch (baud) {
	case TFMINI_BAUD_9600:
	case TFMINI_BAUD_14400:
	case TFMINI_BAUD_19200:
	case TFMINI_BAUD_38400:
	case TFMINI_BAUD_56000:
	case TFMINI_BAUD_57600:
	case TFMINI_BAUD_115200:
	case TFMINI_BAUD_128000:
	case TFMINI_BAUD_230400:
	case TFMINI_BAUD_256000:
	case TFMINI_BAUD_460800:
	case TFMINI_BAUD_500000:
	case TFMINI_BAUD_512000:
		break;
	default:
		return 0;
	}

	p[0] = TFMINI_CMD_HEADER;
	p[1] = 0x08;
	p[2] = 0x06;
	write_u32_le(&p[3], (uint32_t)baud);
	return cmd_write(out, out_cap, p, sizeof(p));
}

size_t tfmini_cmd_set_output_format(uint8_t *out, size_t out_cap, tfmini_output_format_t fmt)
{
	uint8_t p[] = { TFMINI_CMD_HEADER, 0x05, 0x05, (uint8_t)fmt };
	return cmd_write(out, out_cap, p, sizeof(p));
}

size_t tfmini_cmd_set_output_enable(uint8_t *out, size_t out_cap, bool enable)
{
	uint8_t p[] = { TFMINI_CMD_HEADER, 0x05, 0x07, enable ? 0x01u : 0x00u };
	return cmd_write(out, out_cap, p, sizeof(p));
}

size_t tfmini_cmd_save_settings(uint8_t *out, size_t out_cap)
{
	const uint8_t p[] = { TFMINI_CMD_HEADER, 0x04, 0x11 };
	return cmd_write(out, out_cap, p, sizeof(p));
}
