#include <iostream>
#include <kcpev.h>
#include <gtest/gtest.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <ringbuf.h>

#ifdef __cplusplus
}
#endif

using namespace std;

TEST(RingbufTest, baseTest)
{
    ringbuf *rb = ringbuf_new(5);

    EXPECT_EQ(ringbuf_put(rb, "aaaa", 4), 0);

    EXPECT_EQ(ringbuf_get_pending_size(rb), 4);

    char *data, *data1;
    EXPECT_EQ(ringbuf_get_next_chunk(rb, &data), 4);
    EXPECT_EQ(ringbuf_get_next_chunk(rb, &data1), 4);
    EXPECT_EQ(data, data1);

    ringbuf_mark_consumed(rb, 3);
    EXPECT_EQ(ringbuf_get_pending_size(rb), 1);

    EXPECT_EQ(ringbuf_get_pending_size(rb), 1);

    EXPECT_EQ(ringbuf_get_next_chunk(rb, &data), 1);
    EXPECT_EQ(data, data1 + 3);

    EXPECT_EQ(ringbuf_put(rb, "bbbb", 4), 0);
    EXPECT_EQ(ringbuf_get_pending_size(rb), 5);

    EXPECT_EQ(ringbuf_get_pending_size(rb), 5);

    EXPECT_EQ(ringbuf_get_next_chunk(rb, &data), 2);

    char ret[5];
    memcpy(ret, data, 2);
    ringbuf_mark_consumed(rb, 2);
    EXPECT_EQ(ringbuf_get_pending_size(rb), 3);

    EXPECT_EQ(ringbuf_get_next_chunk(rb, &data), 3);
    EXPECT_EQ(data, data1);

    memcpy(ret, data, 3);
    ringbuf_mark_consumed(rb, 3);
    EXPECT_EQ(ringbuf_get_pending_size(rb), 0);

    EXPECT_EQ(ringbuf_get_next_chunk(rb, &data), 0);
    EXPECT_EQ(data, (char *)NULL);
}

int main(int argc, char* argv[])
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

