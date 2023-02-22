#include <gtest/gtest.h>

#include "TFminiPlus.h"

TEST(find_header, case_1)
{
	uint8_t seq[256] = {0x00,0x00, 0x59, 0x59, 0x45, 0x43};
	uint8_t* h_ptr = NULL; 
	uint8_t len = find_header(seq, &h_ptr, 6);
	ASSERT_EQ(*h_ptr, 0x59);
	ASSERT_EQ(*(h_ptr + 1), 0x59);
	ASSERT_EQ(len, 4);
}

TEST(find_header, case_2)
{
	uint8_t* h_ptr = NULL;
	uint8_t len = find_header(NULL, NULL, 6);
	ASSERT_EQ(len, 0);
	EXPECT_EQ(h_ptr, nullptr);
}

TEST(find_header, case_3)
{
	uint8_t seq[256] = {0x00,0x00, 0x59, 0x59, 0x45, 0x43};
	uint8_t* h_ptr = NULL;
	uint8_t len = find_header(seq, &h_ptr, 0);
	ASSERT_EQ(len, 0);
	EXPECT_EQ(h_ptr, nullptr);
}

TEST(find_header, case_4)
{
	uint8_t* h_ptr = NULL;
	uint8_t len = find_header(NULL, &h_ptr, 6);

	EXPECT_EQ(h_ptr, nullptr);
}

TEST(input_data, case_1)
{
	uint8_t data[256] = {0x23, 0x56, 0x59, 0x59, 0x01, 0xF6, 0x07, 0x05, 0x45, 0x76, 0x70};
	d = 0; s = 0; t = 0;
	input_data(data, 11);
	ASSERT_EQ(d, 0xF601);
	ASSERT_EQ(s, 0x0507);
	ASSERT_EQ(t, 0x7645);	
}

TEST(input_data, case_2)
{
	
	uint8_t data_1[256] = {0x23, 0x56, 0x59, 0x59, 0x01};
	uint8_t data_2[256] = {0xF6, 0x07, 0x05, 0x45,};
	uint8_t data_3[256] = {0x76, 0x70};
	d = 0; s = 0; t = 0;
	input_data(data_1, 5);
	input_data(data_2, 4);
	input_data(data_3, 2);
	ASSERT_EQ(d, 0xF601);
	ASSERT_EQ(s, 0x0507);
	ASSERT_EQ(t, 0x7645);	
}

TEST(input_data, case_3)
{
	uint8_t data_0[256] = {0x22, 0x00, 0x59, 0x89, 0x02, 0x76, 0x96};
	uint8_t data_1[256] = {0x23, 0x56, 0x59, 0x59, 0x01};
	uint8_t data_2[256] = {0xF6, 0x07, 0x05, 0x45,};
	uint8_t data_3[256] = {0x76, 0x70};
	d = 0; s = 0; t = 0;
	input_data(data_0, 7);
	input_data(data_1, 5);
	input_data(data_2, 4);
	input_data(data_3, 2);
	ASSERT_EQ(d, 0xF601);
	ASSERT_EQ(s, 0x0507);
	ASSERT_EQ(t, 0x7645);	
}

TEST(input_data, case_4)
{
	uint8_t data_0[256] = {0x22, 0x00, 0x59, 0x89, 0x59, 0x59, 0x96};
	uint8_t data_1[256] = {0x23, 0x56, 0x59, 0x59, 0x01};
	uint8_t data_2[256] = {0xF6, 0x07, 0x05, 0x45,};
	uint8_t data_3[256] = {0x76, 0x70};
	d = 0; s = 0; t = 0;
	input_data(data_0, 7);
	input_data(data_1, 5);
	input_data(data_2, 4);
	input_data(data_3, 2);
	ASSERT_NE(d, 0xF601);
	ASSERT_NE(s, 0x0507);
	ASSERT_NE(t, 0x7645);	
	ASSERT_EQ(d, 0);
	ASSERT_EQ(s, 0);
	ASSERT_EQ(t, 0);	
}

TEST(input_data, case_5)
{
	uint8_t data_0[256] = {0};
	uint8_t data_1[256] = {0x23, 0x56, 0x59, 0x59, 0x01};
	uint8_t data_2[256] = {0xF6, 0x07, 0x05, 0x45,};
	uint8_t data_3[256] = {0x76, 0x70};
	d = 0; s = 0; t = 0;
	input_data(NULL, 0);
//	input_data(data_0, 10);
	input_data(data_1, 5);
	input_data(data_2, 4);
	input_data(data_3, 2);
	ASSERT_EQ(d, 0xF601);
	ASSERT_EQ(s, 0x0507);
	ASSERT_EQ(t, 0x7645);	
}
