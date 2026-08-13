/**
 * \file    TFminiPlus.h
 * \brief   TFmini Plus lidar frame parser and configuration helpers.
 *
 * AUTHOR  Roman Garanin
 */

#ifndef TFMINI_PLUS_H
#define TFMINI_PLUS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/** Output measurement frame length (bytes), including headers and checksum. */
#define TFMINI_FRAME_LEN 9u

/** Receive ring-buffer capacity (must be > TFMINI_FRAME_LEN). */
#define TFMINI_RX_BUFFER_SIZE 256u

/** Maximum configuration command length (header + payload + checksum). */
#define TFMINI_CMD_MAX_LEN 16u

typedef enum {
	TFMINI_OK = 0,          /**< At least one valid frame was parsed. */
	TFMINI_NEED_MORE,       /**< Waiting for more bytes. */
	TFMINI_BAD_CHECKSUM,    /**< Checksum failed; stream resynced. */
	TFMINI_OVERFLOW,        /**< Ring buffer would overflow; context reset. */
	TFMINI_INVALID_ARG      /**< NULL pointer or zero-length input. */
} tfmini_status_t;

typedef enum {
	TFMINI_OUT_CM = 0x01,
	TFMINI_OUT_PIX = 0x02,
	TFMINI_OUT_MM = 0x06
} tfmini_output_format_t;

typedef struct {
	uint16_t distance_cm;   /**< Last valid distance (cm), or mm if device set to mm. */
	uint16_t strength;      /**< Last valid signal strength. */
	int16_t  temperature_c; /**< Last valid temperature in °C. */
	bool     has_data;      /**< True after at least one valid frame. */

	/* Internal receive state (do not modify). */
	uint8_t  rx[TFMINI_RX_BUFFER_SIZE];
	size_t   head;
	size_t   tail;
	size_t   count;
} tfmini_plus_t;

/**
 * \brief Initialize (or reset) a parser context.
 */
void tfmini_plus_init(tfmini_plus_t *ctx);

/**
 * \brief Feed UART bytes into the parser.
 *
 * Valid frames update \p ctx->distance_cm, \p ctx->strength, \p ctx->temperature_c.
 * On checksum failure the stream is resynchronized to the next 0x59 0x59 header.
 *
 * \return TFMINI_OK if one or more frames were accepted;
 *         TFMINI_NEED_MORE if no complete valid frame yet;
 *         TFMINI_BAD_CHECKSUM if a frame failed checksum (resync done);
 *         TFMINI_OVERFLOW / TFMINI_INVALID_ARG on error.
 */
tfmini_status_t tfmini_plus_feed(tfmini_plus_t *ctx, const uint8_t *data, size_t len);

/**
 * \brief Build a configuration command into \p out.
 *
 * Caller sends \p out[0 .. returned_len) over UART to the sensor.
 * Returns number of bytes written, or 0 on error.
 */
size_t tfmini_cmd_get_version(uint8_t *out, size_t out_cap);
size_t tfmini_cmd_system_reset(uint8_t *out, size_t out_cap);
size_t tfmini_cmd_set_frame_rate(uint8_t *out, size_t out_cap, uint16_t hz);
size_t tfmini_cmd_set_output_format(uint8_t *out, size_t out_cap, tfmini_output_format_t fmt);
size_t tfmini_cmd_set_output_enable(uint8_t *out, size_t out_cap, bool enable);
size_t tfmini_cmd_save_settings(uint8_t *out, size_t out_cap);

#ifdef __cplusplus
}
#endif

#endif /* TFMINI_PLUS_H */
