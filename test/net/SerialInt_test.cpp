/**
 * This file tests class `SerialInt`.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *       File: SerialInt_test.cpp
 * Created On: Nov 3, 2017
 *     Author: Steven R. Emmerson
 */
#include "config.h"

#include "SerialInt.h"

#include <gtest/gtest.h>
#include <stdint.h>

namespace {

/// The fixture for testing class `SerialInt`
class SerialIntTest : public ::testing::Test
{
protected:
    SerialIntTest()
    {
        // You can do set-up work for each test here.
    }

    virtual ~SerialIntTest()
    {
        // You can do clean-up work that doesn't throw exceptions here.
    }

    virtual void SetUp()
    {
        // Code here will be called immediately after the constructor (right
        // before each test).
    }

    virtual void TearDown()
    {
        // Code here will be called immediately after each test (right
        // before the destructor).
    }
};

// Tests default construction
TEST_F(SerialIntTest, DefaultConstruction)
{
    hycast::SerialInt<uint8_t> u8{};
    EXPECT_EQ(0, u8);
    EXPECT_EQ(1, u8.getSerialSize(0));
    hycast::SerialInt<uint16_t> u16{};
    EXPECT_EQ(0, u16);
    EXPECT_EQ(2, u16.getSerialSize(0));
    hycast::SerialInt<uint32_t> u32{};
    EXPECT_EQ(0, u32);
    EXPECT_EQ(4, u32.getSerialSize(0));
    hycast::SerialInt<uint64_t> u64{};
    EXPECT_EQ(0, u64);
    EXPECT_EQ(8, u64.getSerialSize(0));
}

// Tests construction
TEST_F(SerialIntTest, Construction)
{
    hycast::SerialInt<uint8_t> u8{1};
    EXPECT_EQ(1, u8);
    hycast::SerialInt<uint16_t> u16{1};
    EXPECT_EQ(1, u16);
    hycast::SerialInt<uint32_t> u32{1};
    EXPECT_EQ(1, u32);
    hycast::SerialInt<uint64_t> u64{1};
    EXPECT_EQ(1, u64);
}

// Tests uint8 serialization
TEST_F(SerialIntTest, Uint8Serialization)
{
    char buf[256];
    hycast::SerialInt<uint8_t> u8{1};
    auto encoder = hycast::MemEncoder(buf, sizeof(buf));
    u8.serialize(encoder, 0);
    encoder.flush();
    auto decoder = hycast::MemDecoder(buf, sizeof(buf));
    decoder.fill(0);
    EXPECT_EQ(1, u8.deserialize(decoder, 0));
}

// Tests uint16 serialization
TEST_F(SerialIntTest, Uint16Serialization)
{
    char buf[256];
    hycast::SerialInt<uint16_t> u16{1};
    auto encoder = hycast::MemEncoder(buf, sizeof(buf));
    u16.serialize(encoder, 0);
    encoder.flush();
    auto decoder = hycast::MemDecoder(buf, sizeof(buf));
    decoder.fill(0);
    EXPECT_EQ(1, u16.deserialize(decoder, 0));
}

// Tests uint32 serialization
TEST_F(SerialIntTest, Uint32Serialization)
{
    char buf[256];
    hycast::SerialInt<uint32_t> u32{1};
    auto encoder = hycast::MemEncoder(buf, sizeof(buf));
    u32.serialize(encoder, 0);
    encoder.flush();
    auto decoder = hycast::MemDecoder(buf, sizeof(buf));
    decoder.fill(0);
    EXPECT_EQ(1, u32.deserialize(decoder, 0));
}

// Tests uint64 serialization
TEST_F(SerialIntTest, Uint64Serialization)
{
    char buf[256];
    hycast::SerialInt<uint64_t> u64{1};
    auto encoder = hycast::MemEncoder(buf, sizeof(buf));
    u64.serialize(encoder, 0);
    encoder.flush();
    auto decoder = hycast::MemDecoder(buf, sizeof(buf));
    decoder.fill(0);
    EXPECT_EQ(1, u64.deserialize(decoder, 0));
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
