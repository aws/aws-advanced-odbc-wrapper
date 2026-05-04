// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <gtest/gtest.h>
#include "../../driver/util/rds_strings.h"

class RdsStringsTest : public testing::Test {};

#ifdef UNICODE

TEST_F(RdsStringsTest, SimpleConversion) {
    SQLTCHAR in[] = {97, 0, 98, 0, 0, 0};
    SQLTCHAR out[3] = {0};

    Convert4To2ByteString(true, in, out, 3);

    EXPECT_EQ(out[0], 97);
    EXPECT_EQ(out[1], 98);
    EXPECT_EQ(out[2], 0);
}

TEST_F(RdsStringsTest, SurrogatePairConversion) {
    // Rocket emoji as utf-32: 0x0001F680
    // Each UChar32 occupies 2 SQLTCHAR elements (little-endian layout).
    // We need the output buffer large enough for the surrogate pair + null.
    SQLTCHAR in[] = {0xF680, 0x0001, 0x0000, 0x0000};
    SQLTCHAR out[4] = {0};

    Convert4To2ByteString(true, in, out, 4);

    // Surrogate pair: high=0xD83D, low=0xDE80
    EXPECT_EQ(out[0], 0xD83D);
    EXPECT_EQ(out[1], 0xDE80);
    EXPECT_EQ(out[2], 0x0000);
}

TEST_F(RdsStringsTest, ConvertInput) {
    SQLTCHAR in[] = {97, 0, 98, 0, 0, 0};

    Convert4To2ByteString(true, in, nullptr, 3);

    EXPECT_EQ(in[0], 97);
    EXPECT_EQ(in[1], 98);
    EXPECT_EQ(in[2], 0);
}

TEST_F(RdsStringsTest, SurrogatePairTruncatedWhenBufferTooSmall) {
    // Rocket emoji U+1F680 as UTF-32 in 2 SQLTCHAR elements (little-endian)
    // followed by 'a' (U+0061) as UTF-32 and null terminator.
    SQLTCHAR in[] = {0xF680, 0x0001, 0x0061, 0x0000, 0x0000, 0x0000};
    // Output buffer only has room for 2 code units + null.
    // The surrogate pair needs 2 code units, so it should fit, but 'a' won't.
    SQLTCHAR out[3] = {0};

    Convert4To2ByteString(true, in, out, 3);

    // Surrogate pair fits exactly in 2 slots
    EXPECT_EQ(out[0], 0xD83D);
    EXPECT_EQ(out[1], 0xDE80);
    EXPECT_EQ(out[2], 0x0000);
}

TEST_F(RdsStringsTest, UnpairedSurrogateDroppedWhenTruncated) {
    // Two supplementary characters: U+1F680 and U+1F4A9
    // Each needs a surrogate pair (2 UTF-16 code units).
    // With output_size=3, ICU would write 4 code units but we clamp to 3,
    // leaving an unpaired high surrogate. The fix should drop it back to 2.
    SQLTCHAR in[] = {0xF680, 0x0001, 0xF4A9, 0x0001, 0x0000, 0x0000};
    SQLTCHAR out[4] = {0};

    Convert4To2ByteString(true, in, out, 4);

    // Only the first complete surrogate pair should be in the output.
    // The second pair's high surrogate should be dropped to avoid corruption.
    EXPECT_EQ(out[0], 0xD83D);
    EXPECT_EQ(out[1], 0xDE80);
    EXPECT_EQ(out[2], 0x0000);
}

TEST_F(RdsStringsTest, GetLenOfSqltcharArray_4Byte_NTS) {
    // "ab" as UChar32: each codepoint takes 2 SQLTCHAR elements
    SQLTCHAR in[] = {97, 0, 98, 0, 0, 0};
    size_t len = GetLenOfSqltcharArray(in, SQL_NTS, true);
    EXPECT_EQ(len, 3u);
}

TEST_F(RdsStringsTest, GetLenOfSqltcharArray_4Byte_NTS_WithSupplementary) {
    // U+1F680 (rocket) as UChar32 in 2 SQLTCHAR elements then null
    SQLTCHAR in[] = {0xF680, 0x0001, 0x0000, 0x0000};
    size_t len = GetLenOfSqltcharArray(in, SQL_NTS, true);
    EXPECT_EQ(len, 3u);
}

TEST_F(RdsStringsTest, GetLenOfSqltcharArray_4Byte_NTS_MixedBmpAndSupplementary) {
    // 'a' (U+0061) as UChar32, then U+1F680 (rocket) as UChar32, then null
    SQLTCHAR in[] = {0x0061, 0x0000, 0xF680, 0x0001, 0x0000, 0x0000};
    size_t len = GetLenOfSqltcharArray(in, SQL_NTS, true);
    EXPECT_EQ(len, 4u);
}

TEST_F(RdsStringsTest, NullIn) {
    SQLTCHAR out[4] = {97, 97, 97, 97};
    Convert4To2ByteString(true, nullptr, out, 4);
    EXPECT_EQ(out[0], 97);
}

TEST_F(RdsStringsTest, Convert2ByteChars) {
    SQLTCHAR in[] = {97, 98, 99, 0};
    SQLTCHAR out[4] = {0};

    Convert4To2ByteString(false, in, out, 4);

    EXPECT_EQ(out[0], 97);
    EXPECT_EQ(out[1], 98);
    EXPECT_EQ(out[2], 99);
    EXPECT_EQ(out[3], 0);
}

TEST_F(RdsStringsTest, GetLenOfSqltcharArray_2Byte_NTS) {
    SQLTCHAR in[] = {97, 98, 99, 0};
    size_t len = GetLenOfSqltcharArray(in, SQL_NTS, false);
    // Should include null terminator: 3 chars + 1 null = 4
    EXPECT_EQ(len, 4u);
}

TEST_F(RdsStringsTest, GetLenOfSqltcharArray_ExplicitLength) {
    SQLTCHAR in[] = {97, 98, 99, 0};
    size_t len = GetLenOfSqltcharArray(in, 10, false);
    EXPECT_EQ(len, 11u);
}

TEST_F(RdsStringsTest, GetLenOfSqltcharArray_ZeroLength) {
    SQLTCHAR in[] = {97, 0};
    size_t len = GetLenOfSqltcharArray(in, 0, false);
    EXPECT_EQ(len, 0u);
}

TEST_F(RdsStringsTest, GetLenOfSqltcharArray_4Byte_ExplicitLength_BMP) {
    // "abc" as UChar32: 3 codepoints, each BMP, so UTF-16 length = 3 + 1 null = 4
    SQLTCHAR in[] = {0x0061, 0x0000, 0x0062, 0x0000, 0x0063, 0x0000};
    size_t len = GetLenOfSqltcharArray(in, 3, true);
    EXPECT_EQ(len, 4u);
}

TEST_F(RdsStringsTest, GetLenOfSqltcharArray_4Byte_ExplicitLength_Supplementary) {
    // U+1F680 (rocket) as UChar32: 1 codepoint, needs surrogate pair = 2 UTF-16 units + 1 null = 3
    SQLTCHAR in[] = {0xF680, 0x0001, 0x0000, 0x0000};
    size_t len = GetLenOfSqltcharArray(in, 1, true);
    EXPECT_EQ(len, 3u);
}

TEST_F(RdsStringsTest, GetLenOfSqltcharArray_4Byte_ExplicitLength_Mixed) {
    // 'a' (U+0061) then U+1F680: 2 codepoints, UTF-16 = 1 + 2 = 3 units + 1 null = 4
    SQLTCHAR in[] = {0x0061, 0x0000, 0xF680, 0x0001, 0x0000, 0x0000};
    size_t len = GetLenOfSqltcharArray(in, 2, true);
    EXPECT_EQ(len, 4u);
}

#endif // UNICODE
