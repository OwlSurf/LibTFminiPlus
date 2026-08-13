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

static int16_t temp_raw_to_c(uint16_t raw)
{
	/* Product manual: Temp(°C) = Temp_raw / 8 - 256 */
	return (int16_t)((int32_t)raw / 8 - 256);
}

static void ring_reset(tfmini_plus_t *ctx)
{
	ctx->head = 0;
	ctx->tail = 0;
	ctx->count = 0;
}

static size_t ring_free(const tfmini_plus_t *ctx)
{
	return TFMINI_RX_BUFFER_SIZE - ctx->count;
}

static bool ring_push(tfmini_plus_t *ctx, const uint8_t *data, size_t len)
{
	if (len > ring_free(ctx)) {
		return false;
	}
	for (size_t i = 0; i < len; i++) {
		ctx->rx[ctx->tail] = data[i];
		ctx->tail = (ctx->tail + 1u) % TFMINI_RX_BUFFER_SIZE;
	}
	ctx->count += len;
	return true;
}

static uint8_t ring_at(const tfmini_plus_t *ctx, size_t offset)
{
	return ctx->rx[(ctx->head + offset) % TFMINI_RX_BUFFER_SIZE];
}

static void ring_discard(tfmini_plus_t *ctx, size_t n)
{
	if (n > ctx->count) {
		n = ctx->count;
	}
	ctx->head = (ctx->head + n) % TFMINI_RX_BUFFER_SIZE;
	ctx->count -= n;
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
	while (ctx->count >= 2u) {
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
	if (ctx->count < TFMINI_FRAME_LEN) {
		return false;
	}

	ring_copy_frame(ctx, frame);

	if (frame_checksum(frame) != frame[8]) {
		*checksum_failed = true;
		/* Skip first header byte and resync on next opportunity. */
		ring_discard(ctx, 1);
		return false;
	}

	ctx->distance_cm = read_u16_le(&frame[2]);
	ctx->strength = read_u16_le(&frame[4]);
	ctx->temperature_c = temp_raw_to_c(read_u16_le(&frame[6]));
	ctx->has_data = true;

	ring_discard(ctx, TFMINI_FRAME_LEN);
	return true;
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
	bool saw_ok = false;
	bool saw_bad_cs = false;

	if (ctx == NULL || data == NULL || len == 0) {
		return TFMINI_INVALID_ARG;
	}

	if (!ring_push(ctx, data, len)) {
		ring_reset(ctx);
		return TFMINI_OVERFLOW;
	}

	for (;;) {
		bool checksum_failed = false;
		if (try_parse_one_frame(ctx, &checksum_failed)) {
			saw_ok = true;
			continue;
		}
		if (checksum_failed) {
			saw_bad_cs = true;
			continue; /* keep trying to resync / parse next */
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
	uint8_t p[] = {
		TFMINI_CMD_HEADER, 0x06, 0x03,
		(uint8_t)(hz & 0xFFu),
		(uint8_t)((hz >> 8) & 0xFFu)
	};
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
