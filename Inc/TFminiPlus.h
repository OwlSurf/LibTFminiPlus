/**
 * \file    TFminiPlus.h
 * \brief   TFmini Plus lidar frame parser and configuration helpers.
 *
 * AUTHOR  Roman Garanin
 *
 * Thread-safety: not thread-safe. Protect a shared tfmini_plus_t with an
 * external lock if feed()/readers run concurrently.
 *
 * RX path parses measurement frames only (0x59 0x59 ...). Command helpers
 * only build TX payloads; command ACK/version replies are not parsed.
 *
 * Optional: define TFMINI_RX_BUFFER_SIZE before including this header
 * (must be >= 2 * TFMINI_FRAME_LEN). Default is 256.
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

#ifndef TFMINI_RX_BUFFER_SIZE
/** Receive ring-buffer capacity (must be >= 2 * TFMINI_FRAME_LEN). */
#define TFMINI_RX_BUFFER_SIZE 256u
#endif

#if TFMINI_RX_BUFFER_SIZE < (2u * TFMINI_FRAME_LEN)
#error "TFMINI_RX_BUFFER_SIZE must be >= 2 * TFMINI_FRAME_LEN"
#endif

/** Maximum configuration command length (header + payload + checksum). */
#define TFMINI_CMD_MAX_LEN 16u

/** Typical valid signal-strength window (Benewake guidance). */
#define TFMINI_STRENGTH_MIN 100u
#define TFMINI_STRENGTH_MAX 3000u

/** Distance sentinel often used for no-measure / out-of-range. */
#define TFMINI_DIST_INVALID 0xFFFFu

/** Supported frame-rate limits (Hz) for tfmini_cmd_set_frame_rate. */
#define TFMINI_FRAME_RATE_MIN 1u
#define TFMINI_FRAME_RATE_MAX 1000u

typedef enum {
	TFMINI_OK = 0,          /**< At least one valid frame was parsed. */
	TFMINI_NEED_MORE,       /**< Waiting for more bytes (incl. len == 0). */
	TFMINI_BAD_CHECKSUM,    /**< Checksum failed; stream resynced. */
	TFMINI_OVERFLOW,        /**< Ring reset; incoming chunk re-ingested if possible. */
	TFMINI_INVALID_ARG      /**< NULL pointer or other invalid argument. */
} tfmini_status_t;

typedef enum {
	TFMINI_OUT_CM = 0x01,
	TFMINI_OUT_PIX = 0x02,
	TFMINI_OUT_MM = 0x06
} tfmini_output_format_t;

typedef enum {
	TFMINI_BAUD_9600 = 9600,
	TFMINI_BAUD_14400 = 14400,
	TFMINI_BAUD_19200 = 19200,
	TFMINI_BAUD_38400 = 38400,
	TFMINI_BAUD_56000 = 56000,
	TFMINI_BAUD_57600 = 57600,
	TFMINI_BAUD_115200 = 115200,
	TFMINI_BAUD_128000 = 128000,
	TFMINI_BAUD_230400 = 230400,
	TFMINI_BAUD_256000 = 256000,
	TFMINI_BAUD_460800 = 460800,
	TFMINI_BAUD_500000 = 500000,
	TFMINI_BAUD_512000 = 512000
} tfmini_baud_t;

typedef struct {
	uint16_t distance;      /**< Last valid distance (cm/mm/pix per device format). */
	uint16_t strength;      /**< Last valid signal strength. */
	int16_t  temperature_c; /**< Last valid temperature in °C. */
	bool     has_data;      /**< True after at least one valid frame. */

	uint32_t frames_ok;            /**< Accepted measurement frames. */
	uint32_t frames_bad_checksum;  /**< Frames rejected by checksum. */
	uint32_t overflows;            /**< Ring overflow events. */

	/* Internal receive state — do not read or write. */
	struct {
		uint8_t rx[TFMINI_RX_BUFFER_SIZE];
		size_t  head;
		size_t  tail;
		size_t  count;
	} _;
} tfmini_plus_t;

/**
 * \brief Initialize (or reset) a parser context.
 */
void tfmini_plus_init(tfmini_plus_t *ctx);

/**
 * \brief Feed UART bytes into the parser.
 *
 * Valid frames update distance / strength / temperature_c.
 * On checksum failure the stream is resynchronized to the next 0x59 0x59 header.
 * \p len == 0 with non-NULL \p ctx/\p data returns TFMINI_NEED_MORE.
 *
 * \return TFMINI_OK if one or more frames were accepted;
 *         TFMINI_NEED_MORE if no complete valid frame yet;
 *         TFMINI_BAD_CHECKSUM if a frame failed checksum (and none succeeded);
 *         TFMINI_OVERFLOW if the ring overflowed (chunk may still be parsed);
 *         TFMINI_INVALID_ARG on NULL pointers.
 */
tfmini_status_t tfmini_plus_feed(tfmini_plus_t *ctx, const uint8_t *data, size_t len);

/**
 * \brief True if last distance looks like a real measurement (not 0 / 0xFFFF).
 */
bool tfmini_plus_distance_valid(const tfmini_plus_t *ctx);

/**
 * \brief True if last strength is within TFMINI_STRENGTH_MIN..MAX.
 */
bool tfmini_plus_strength_valid(const tfmini_plus_t *ctx);

/**
 * \brief True if has_data and both distance and strength look valid.
 */
bool tfmini_plus_measurement_valid(const tfmini_plus_t *ctx);

/**
 * \brief Human-readable status string (never NULL).
 */
const char *tfmini_status_str(tfmini_status_t status);

/**
 * \brief Build a configuration command into \p out.
 *
 * Caller sends \p out[0 .. returned_len) over UART to the sensor.
 * Returns number of bytes written, or 0 on error.
 */
size_t tfmini_cmd_get_version(uint8_t *out, size_t out_cap);
size_t tfmini_cmd_system_reset(uint8_t *out, size_t out_cap);
size_t tfmini_cmd_set_frame_rate(uint8_t *out, size_t out_cap, uint16_t hz);
size_t tfmini_cmd_set_baud_rate(uint8_t *out, size_t out_cap, tfmini_baud_t baud);
size_t tfmini_cmd_set_output_format(uint8_t *out, size_t out_cap, tfmini_output_format_t fmt);
size_t tfmini_cmd_set_output_enable(uint8_t *out, size_t out_cap, bool enable);
size_t tfmini_cmd_save_settings(uint8_t *out, size_t out_cap);

#ifdef __cplusplus
}
#endif

#endif /* TFMINI_PLUS_H */
