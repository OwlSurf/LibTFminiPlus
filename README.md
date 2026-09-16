TFminiPlus Library
==================

## Brief

Library for Benewake **TFmini Plus** lidar: parse UART measurement frames and build configuration commands.

## Features

- Streaming parser with ring buffer (handles split frames, wrap-around, and noise)
- Checksum validation with header resync
- Per-instance context (no globals; multiple sensors OK)
- Temperature converted to °C (`Temp_raw / 8 − 256`)
- Measurement validity helpers (distance / strength)
- Frame / checksum / overflow counters
- Helpers for config commands (frame rate, baud rate, output format, save, reset)
- Configurable RX buffer size via `TFMINI_RX_BUFFER_SIZE`

## Limits

- **Not thread-safe.** Protect a shared `tfmini_plus_t` if `feed()` and readers run concurrently.
- **RX parses measurement frames only** (`0x59 0x59` …). Command helpers build **TX** payloads only; ACK / version replies are not parsed.
- On ring overflow the buffer is reset and the **incoming chunk is re-ingested** when it fits; status is still `TFMINI_OVERFLOW`.

## How to use

1. Add this repository to your project as a submodule (or copy `Src/` + `Inc/`).
2. Compile `Src/TFminiPlus.c` and add `Inc/` to the include path.
3. Or with CMake:

```cmake
add_subdirectory(path/to/LibTFminiPlus)
target_link_libraries(your_app PRIVATE TFminiPlus)
```

Optional smaller RX buffer (define **before** include / via `-D`):

```c
#define TFMINI_RX_BUFFER_SIZE 64
#include "TFminiPlus.h"
```

### Example (UART RX)

```c
#include "TFminiPlus.h"

static tfmini_plus_t lidar;

void app_init(void)
{
    uint8_t cmd[TFMINI_CMD_MAX_LEN];
    size_t n;

    tfmini_plus_init(&lidar);

    n = tfmini_cmd_set_frame_rate(cmd, sizeof(cmd), 100);
    uart_write(cmd, n); /* your UART TX */

    n = tfmini_cmd_set_baud_rate(cmd, sizeof(cmd), TFMINI_BAUD_115200);
    uart_write(cmd, n);

    n = tfmini_cmd_save_settings(cmd, sizeof(cmd));
    uart_write(cmd, n);
}

void uart_rx_callback(const uint8_t *bytes, size_t len)
{
    tfmini_status_t st = tfmini_plus_feed(&lidar, bytes, len);
    if (st == TFMINI_OK && tfmini_plus_measurement_valid(&lidar)) {
        /* lidar.distance, lidar.strength, lidar.temperature_c */
    }
}
```

### Measurement frame (9 bytes)

| Bytes | Content |
|-------|---------|
| 0–1   | `0x59 0x59` header |
| 2–3   | Distance (little-endian) |
| 4–5   | Strength (little-endian) |
| 6–7   | Temperature raw (little-endian) |
| 8     | Checksum = sum of bytes 0–7 |

## How to build tests

Needs: `cmake`, `gcc`/`g++` (GoogleTest is fetched automatically).

```bash
./build_test.sh
```

Or manually:

```bash
cmake -S gtest/ -B gtest/build
cmake --build gtest/build
ctest --test-dir gtest/build --output-on-failure
```

Binary: `gtest/build/runtest`

## References

1. [SJ-PM-TFmini Plus A05 Product Manual](https://github.com/TFmini/TFmini-Plus)
