#include <gtest/gtest.h>

#include <cstring>
#include <vector>

#include "TFminiPlus.h"

namespace {

uint8_t sum8(const uint8_t *p, size_t n)
{
	uint8_t s = 0;
	for (size_t i = 0; i < n; i++) {
		s = static_cast<uint8_t>(s + p[i]);
	}
	return s;
}

/** Build a valid 9-byte frame. temp_c is converted to raw per manual. */
std::vector<uint8_t> make_frame(uint16_t dist, uint16_t strength, int16_t temp_c)
{
	uint16_t temp_raw = static_cast<uint16_t>((static_cast<int32_t>(temp_c) + 256) * 8);
	std::vector<uint8_t> f = {
		0x59, 0x59,
		static_cast<uint8_t>(dist & 0xFF),
		static_cast<uint8_t>((dist >> 8) & 0xFF),
		static_cast<uint8_t>(strength & 0xFF),
		static_cast<uint8_t>((strength >> 8) & 0xFF),
		static_cast<uint8_t>(temp_raw & 0xFF),
		static_cast<uint8_t>((temp_raw >> 8) & 0xFF),
		0
	};
	f[8] = sum8(f.data(), 8);
	return f;
}

} // namespace

TEST(Init, NullSafe)
{
	tfmini_plus_init(nullptr);
}

TEST(Feed, InvalidArgs)
{
	tfmini_plus_t ctx;
	tfmini_plus_init(&ctx);
	uint8_t b = 0x59;
	EXPECT_EQ(tfmini_plus_feed(nullptr, &b, 1), TFMINI_INVALID_ARG);
	EXPECT_EQ(tfmini_plus_feed(&ctx, nullptr, 1), TFMINI_INVALID_ARG);
	EXPECT_EQ(tfmini_plus_feed(&ctx, &b, 0), TFMINI_NEED_MORE);
}

TEST(Feed, SingleCompleteFrame)
{
	tfmini_plus_t ctx;
	tfmini_plus_init(&ctx);
	auto frame = make_frame(0xF601, 0x0507, 25);

	std::vector<uint8_t> stream = {0x23, 0x56};
	stream.insert(stream.end(), frame.begin(), frame.end());

	EXPECT_EQ(tfmini_plus_feed(&ctx, stream.data(), stream.size()), TFMINI_OK);
	EXPECT_TRUE(ctx.has_data);
	EXPECT_EQ(ctx.distance, 0xF601);
	EXPECT_EQ(ctx.strength, 0x0507);
	EXPECT_EQ(ctx.temperature_c, 25);
	EXPECT_EQ(ctx.frames_ok, 1u);
}

TEST(Feed, SplitAcrossCalls)
{
	tfmini_plus_t ctx;
	tfmini_plus_init(&ctx);
	auto frame = make_frame(0xF601, 0x0507, 30);

	std::vector<uint8_t> stream = {0x23, 0x56};
	stream.insert(stream.end(), frame.begin(), frame.end());

	EXPECT_EQ(tfmini_plus_feed(&ctx, stream.data(), 5), TFMINI_NEED_MORE);
	EXPECT_EQ(tfmini_plus_feed(&ctx, stream.data() + 5, 4), TFMINI_NEED_MORE);
	EXPECT_EQ(tfmini_plus_feed(&ctx, stream.data() + 9, 2), TFMINI_OK);
	EXPECT_EQ(ctx.distance, 0xF601);
	EXPECT_EQ(ctx.strength, 0x0507);
	EXPECT_EQ(ctx.temperature_c, 30);
}

TEST(Feed, NoiseThenValid)
{
	tfmini_plus_t ctx;
	tfmini_plus_init(&ctx);
	auto frame = make_frame(100, 200, 20);

	std::vector<uint8_t> stream = {0x22, 0x00, 0x59, 0x89, 0x02, 0x76, 0x96};
	stream.insert(stream.end(), frame.begin(), frame.end());

	EXPECT_EQ(tfmini_plus_feed(&ctx, stream.data(), stream.size()), TFMINI_OK);
	EXPECT_EQ(ctx.distance, 100);
	EXPECT_EQ(ctx.strength, 200);
	EXPECT_EQ(ctx.temperature_c, 20);
}

TEST(Feed, FalseHeaderThenValid)
{
	tfmini_plus_t ctx;
	tfmini_plus_init(&ctx);
	auto frame = make_frame(150, 300, 22);

	std::vector<uint8_t> bad = {0x59, 0x59, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x00};
	std::vector<uint8_t> stream = bad;
	stream.insert(stream.end(), frame.begin(), frame.end());

	EXPECT_EQ(tfmini_plus_feed(&ctx, stream.data(), stream.size()), TFMINI_OK);
	EXPECT_TRUE(ctx.has_data);
	EXPECT_EQ(ctx.distance, 150);
	EXPECT_EQ(ctx.strength, 300);
	EXPECT_EQ(ctx.temperature_c, 22);
	EXPECT_GE(ctx.frames_bad_checksum, 1u);
	EXPECT_EQ(ctx.frames_ok, 1u);
}

TEST(Feed, BadChecksumOnly)
{
	tfmini_plus_t ctx;
	tfmini_plus_init(&ctx);
	std::vector<uint8_t> bad = {0x59, 0x59, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x00};
	EXPECT_EQ(tfmini_plus_feed(&ctx, bad.data(), bad.size()), TFMINI_BAD_CHECKSUM);
	EXPECT_FALSE(ctx.has_data);
	EXPECT_EQ(ctx.frames_bad_checksum, 1u);
}

TEST(Feed, ThreeBadHeadersThenValid)
{
	tfmini_plus_t ctx;
	tfmini_plus_init(&ctx);
	std::vector<uint8_t> bad = {0x59, 0x59, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x00};
	auto frame = make_frame(33, 150, 18);
	std::vector<uint8_t> stream;
	for (int i = 0; i < 3; i++) {
		stream.insert(stream.end(), bad.begin(), bad.end());
	}
	stream.insert(stream.end(), frame.begin(), frame.end());

	EXPECT_EQ(tfmini_plus_feed(&ctx, stream.data(), stream.size()), TFMINI_OK);
	EXPECT_EQ(ctx.distance, 33);
	EXPECT_GE(ctx.frames_bad_checksum, 3u);
}

TEST(Feed, TwoFramesBackToBack)
{
	tfmini_plus_t ctx;
	tfmini_plus_init(&ctx);
	auto f1 = make_frame(10, 11, 15);
	auto f2 = make_frame(20, 21, 16);
	std::vector<uint8_t> stream = f1;
	stream.insert(stream.end(), f2.begin(), f2.end());

	EXPECT_EQ(tfmini_plus_feed(&ctx, stream.data(), stream.size()), TFMINI_OK);
	EXPECT_EQ(ctx.distance, 20);
	EXPECT_EQ(ctx.strength, 21);
	EXPECT_EQ(ctx.temperature_c, 16);
	EXPECT_EQ(ctx.frames_ok, 2u);
}

TEST(Feed, ExactFrameBoundary)
{
	tfmini_plus_t ctx;
	tfmini_plus_init(&ctx);
	auto frame = make_frame(42, 99, 27);
	EXPECT_EQ(tfmini_plus_feed(&ctx, frame.data(), frame.size()), TFMINI_OK);
	EXPECT_EQ(ctx.distance, 42);
}

TEST(Feed, SingleByteChunks)
{
	tfmini_plus_t ctx;
	tfmini_plus_init(&ctx);
	auto frame = make_frame(7, 8, 9);
	tfmini_status_t last = TFMINI_NEED_MORE;
	for (size_t i = 0; i < frame.size(); i++) {
		last = tfmini_plus_feed(&ctx, &frame[i], 1);
		if (i + 1 < frame.size()) {
			EXPECT_EQ(last, TFMINI_NEED_MORE);
		}
	}
	EXPECT_EQ(last, TFMINI_OK);
	EXPECT_EQ(ctx.distance, 7);
	EXPECT_EQ(ctx.temperature_c, 9);
}

TEST(Feed, NegativeTemperature)
{
	tfmini_plus_t ctx;
	tfmini_plus_init(&ctx);
	auto frame = make_frame(50, 200, -10);
	EXPECT_EQ(tfmini_plus_feed(&ctx, frame.data(), frame.size()), TFMINI_OK);
	EXPECT_EQ(ctx.temperature_c, -10);
}

TEST(Feed, OverflowReingestsChunk)
{
	tfmini_plus_t ctx;
	tfmini_plus_init(&ctx);
	auto frame = make_frame(1, 200, 3);

	/* Occupy 8 bytes so free = BUFFER - 8. */
	EXPECT_EQ(tfmini_plus_feed(&ctx, frame.data(), 8), TFMINI_NEED_MORE);

	/* Chunk larger than free, but fits in an empty ring; ends with a valid frame. */
	const size_t chunk_len = (TFMINI_RX_BUFFER_SIZE - 8) + 1;
	ASSERT_LE(chunk_len, static_cast<size_t>(TFMINI_RX_BUFFER_SIZE));
	ASSERT_GE(chunk_len, frame.size());

	std::vector<uint8_t> chunk(chunk_len - frame.size(), 0x00);
	chunk.insert(chunk.end(), frame.begin(), frame.end());

	EXPECT_EQ(tfmini_plus_feed(&ctx, chunk.data(), chunk.size()), TFMINI_OVERFLOW);
	EXPECT_EQ(ctx.overflows, 1u);
	EXPECT_TRUE(ctx.has_data);
	EXPECT_EQ(ctx.distance, 1);
	EXPECT_EQ(ctx.frames_ok, 1u);
}

TEST(Feed, OverflowPreservesHasDataFromPriorFrame)
{
	tfmini_plus_t ctx;
	tfmini_plus_init(&ctx);
	auto frame = make_frame(77, 200, 21);
	EXPECT_EQ(tfmini_plus_feed(&ctx, frame.data(), frame.size()), TFMINI_OK);
	EXPECT_TRUE(ctx.has_data);
	EXPECT_EQ(ctx.distance, 77);

	EXPECT_EQ(tfmini_plus_feed(&ctx, frame.data(), 8), TFMINI_NEED_MORE);
	std::vector<uint8_t> too_big(TFMINI_RX_BUFFER_SIZE - 8 + 1, 0x00);
	EXPECT_EQ(tfmini_plus_feed(&ctx, too_big.data(), too_big.size()), TFMINI_OVERFLOW);
	EXPECT_TRUE(ctx.has_data);
	EXPECT_EQ(ctx.distance, 77);
	EXPECT_EQ(ctx.overflows, 1u);
}

TEST(Feed, RingWrapAround)
{
	tfmini_plus_t ctx;
	tfmini_plus_init(&ctx);
	auto frame = make_frame(12, 250, 19);

	/* Advance head/tail near the end of the ring with noise that is discarded. */
	std::vector<uint8_t> noise(TFMINI_RX_BUFFER_SIZE - 4, 0x00);
	EXPECT_EQ(tfmini_plus_feed(&ctx, noise.data(), noise.size()), TFMINI_NEED_MORE);
	/* At most 1 leftover noise byte may remain after sync. Feed a full frame wrapping. */
	EXPECT_EQ(tfmini_plus_feed(&ctx, frame.data(), frame.size()), TFMINI_OK);
	EXPECT_EQ(ctx.distance, 12);
	EXPECT_EQ(ctx.temperature_c, 19);
}

TEST(Validity, DistanceAndStrength)
{
	tfmini_plus_t ctx;
	tfmini_plus_init(&ctx);
	EXPECT_FALSE(tfmini_plus_measurement_valid(&ctx));

	auto ok = make_frame(100, 500, 20);
	EXPECT_EQ(tfmini_plus_feed(&ctx, ok.data(), ok.size()), TFMINI_OK);
	EXPECT_TRUE(tfmini_plus_distance_valid(&ctx));
	EXPECT_TRUE(tfmini_plus_strength_valid(&ctx));
	EXPECT_TRUE(tfmini_plus_measurement_valid(&ctx));

	auto zero_dist = make_frame(0, 500, 20);
	EXPECT_EQ(tfmini_plus_feed(&ctx, zero_dist.data(), zero_dist.size()), TFMINI_OK);
	EXPECT_FALSE(tfmini_plus_distance_valid(&ctx));
	EXPECT_FALSE(tfmini_plus_measurement_valid(&ctx));

	auto inv = make_frame(TFMINI_DIST_INVALID, 500, 20);
	EXPECT_EQ(tfmini_plus_feed(&ctx, inv.data(), inv.size()), TFMINI_OK);
	EXPECT_FALSE(tfmini_plus_distance_valid(&ctx));

	auto weak = make_frame(100, 50, 20);
	EXPECT_EQ(tfmini_plus_feed(&ctx, weak.data(), weak.size()), TFMINI_OK);
	EXPECT_TRUE(tfmini_plus_distance_valid(&ctx));
	EXPECT_FALSE(tfmini_plus_strength_valid(&ctx));
}

TEST(StatusStr, KnownValues)
{
	EXPECT_STREQ(tfmini_status_str(TFMINI_OK), "OK");
	EXPECT_STREQ(tfmini_status_str(TFMINI_NEED_MORE), "NEED_MORE");
	EXPECT_STREQ(tfmini_status_str(TFMINI_OVERFLOW), "OVERFLOW");
}

TEST(Commands, GetVersion)
{
	uint8_t out[TFMINI_CMD_MAX_LEN];
	size_t n = tfmini_cmd_get_version(out, sizeof(out));
	ASSERT_EQ(n, 4u);
	EXPECT_EQ(out[0], 0x5A);
	EXPECT_EQ(out[1], 0x04);
	EXPECT_EQ(out[2], 0x01);
	EXPECT_EQ(out[3], 0x5F);
}

TEST(Commands, SystemReset)
{
	uint8_t out[TFMINI_CMD_MAX_LEN];
	size_t n = tfmini_cmd_system_reset(out, sizeof(out));
	ASSERT_EQ(n, 4u);
	EXPECT_EQ(out[3], 0x60);
}

TEST(Commands, SetFrameRate)
{
	uint8_t out[TFMINI_CMD_MAX_LEN];
	size_t n = tfmini_cmd_set_frame_rate(out, sizeof(out), 100);
	ASSERT_EQ(n, 6u);
	EXPECT_EQ(out[0], 0x5A);
	EXPECT_EQ(out[1], 0x06);
	EXPECT_EQ(out[2], 0x03);
	EXPECT_EQ(out[3], 100);
	EXPECT_EQ(out[4], 0);
	EXPECT_EQ(out[5], sum8(out, 5));

	EXPECT_EQ(tfmini_cmd_set_frame_rate(out, sizeof(out), 0), 0u);
	EXPECT_EQ(tfmini_cmd_set_frame_rate(out, sizeof(out), 1001), 0u);
	EXPECT_EQ(tfmini_cmd_set_frame_rate(out, sizeof(out), 1), 6u);
	EXPECT_EQ(tfmini_cmd_set_frame_rate(out, sizeof(out), 1000), 6u);
}

TEST(Commands, SetBaudRate)
{
	uint8_t out[TFMINI_CMD_MAX_LEN];
	size_t n = tfmini_cmd_set_baud_rate(out, sizeof(out), TFMINI_BAUD_115200);
	ASSERT_EQ(n, 8u);
	EXPECT_EQ(out[0], 0x5A);
	EXPECT_EQ(out[1], 0x08);
	EXPECT_EQ(out[2], 0x06);
	EXPECT_EQ(out[3], 0x00);
	EXPECT_EQ(out[4], 0xC2);
	EXPECT_EQ(out[5], 0x01);
	EXPECT_EQ(out[6], 0x00);
	EXPECT_EQ(out[7], sum8(out, 7));

	EXPECT_EQ(tfmini_cmd_set_baud_rate(out, sizeof(out), static_cast<tfmini_baud_t>(1234)), 0u);
}

TEST(Commands, SetOutputFormatAndEnable)
{
	uint8_t out[TFMINI_CMD_MAX_LEN];
	size_t n = tfmini_cmd_set_output_format(out, sizeof(out), TFMINI_OUT_CM);
	ASSERT_EQ(n, 5u);
	EXPECT_EQ(out[3], 0x01);

	n = tfmini_cmd_set_output_enable(out, sizeof(out), true);
	ASSERT_EQ(n, 5u);
	EXPECT_EQ(out[3], 0x01);

	n = tfmini_cmd_set_output_enable(out, sizeof(out), false);
	ASSERT_EQ(n, 5u);
	EXPECT_EQ(out[3], 0x00);
}

TEST(Commands, SaveSettings)
{
	uint8_t out[TFMINI_CMD_MAX_LEN];
	size_t n = tfmini_cmd_save_settings(out, sizeof(out));
	ASSERT_EQ(n, 4u);
	EXPECT_EQ(out[2], 0x11);
	EXPECT_EQ(out[3], 0x6F);
}

TEST(Commands, BufferTooSmall)
{
	uint8_t out[2];
	EXPECT_EQ(tfmini_cmd_get_version(out, sizeof(out)), 0u);
	EXPECT_EQ(tfmini_cmd_get_version(nullptr, 16), 0u);
}
