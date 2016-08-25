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
    ringbuf *rb = ringbuf_new(1);
}

int main(int argc, char* argv[])
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

