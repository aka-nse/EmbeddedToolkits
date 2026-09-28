#include <gtest/gtest.h>
#include "platform.h"

namespace embedded { namespace platform {

class RingBufferTest : public ::testing::Test
{
protected:
    template<std::size_t N>
    uint8_t& buffer_at(embedded::platform::RingBuffer<N>& buffer, std::size_t index) {
        return buffer.m_buffer[index];
    }
};


TEST_F(RingBufferTest, initial_state) {
    embedded::platform::RingBuffer<8> buffer;
    EXPECT_TRUE(buffer.empty());
    EXPECT_FALSE(buffer.full());
    EXPECT_EQ(0, buffer.size());
}


TEST_F(RingBufferTest, push_and_pop) {
    embedded::platform::RingBuffer<8> buffer;
    uint8_t value;
    
    // Push values into the buffer
    for (uint8_t i = 0; i < 8; ++i) {
        EXPECT_TRUE(buffer.try_push(i));
    }
    EXPECT_TRUE(buffer.full());
    EXPECT_EQ(8, buffer.size());
    EXPECT_FALSE(buffer.try_push(8)); // Buffer should be full, cannot push more

    // Pop values from the buffer
    for (uint8_t i = 0; i < 8; ++i) {
        EXPECT_TRUE(buffer.try_pop(value));
        EXPECT_EQ(i, value);
    }
    EXPECT_TRUE(buffer.empty());
    EXPECT_EQ(0, buffer.size());
    EXPECT_FALSE(buffer.try_pop(value)); // Buffer should be empty, cannot pop more
}


TEST_F(RingBufferTest, wrap_around) {
    static const size_t buffer_size = 16;
    embedded::platform::RingBuffer<buffer_size> buffer;
    uint8_t value;
    for (uint16_t i = 0; i < 256; ++i) {
        EXPECT_TRUE(buffer.try_push(static_cast<uint8_t>(i)));
        EXPECT_EQ(i, buffer_at(buffer, i % buffer_size));
        EXPECT_TRUE(buffer.try_pop(value));
        EXPECT_EQ(i, value);
    }
}

} }
