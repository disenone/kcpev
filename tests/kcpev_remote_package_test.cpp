#include <kcpev.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <dbg.h>
#include <gtest/gtest.h>
#include <unistd.h>
#include <string>
#include <fcntl.h>
#include <unordered_map>
#include "test.h"

using namespace std;

const char *server_ip = NULL;
const char *server_port = NULL;

void client_recv_cb(Kcpev* kcpev, const char* buf, size_t len)
{
    unordered_map<int, vector<char>> *package_info = static_cast<unordered_map<int, vector<char>> *>(kcpev->data);
    int key = *(int *)buf;
    int ret = 0;
    vector<char>* origin = NULL;

    auto search = package_info->find(key);
    check(search != package_info->end(), "package not found, key: %d", key);

    origin = &package_info->at(key);
    check(origin->size() == len, "package len not match, key: %d, origin: %d, recv: %d", key, origin->size(), len);

    ret = memcmp(origin->data(), buf, len);
    check(ret == 0, "package data not equal, key: %d, origin: %d, recv: %d", key, origin->size(), len);

    package_info->at(key) = vector<char>();

error:
    return;
}

void on_timer(EV_P_ struct ev_timer *w, int revents)
{
    Kcpev *kcpev = (Kcpev *)w->data;
    unordered_map<int, vector<char>> *package_info = static_cast<unordered_map<int, vector<char>> *>(kcpev->data);
    if(package_info->size() > 1000)
        return;

    char buf[KCPEV_BUFFER_SIZE];
    int len = create_rand_str(buf, 4, sizeof(buf));

    *(int *)buf = package_info->size();
    package_info->emplace(package_info->size(), vector<char>(buf, buf + len));

    kcpev_send(kcpev, buf, len);
}

Kcpev* create_client()
{
    Kcpev *kcpev = NULL;
	struct ev_loop *loop = EV_DEFAULT;
    int ret = 0;
    ev_timer *evt = NULL;
    unordered_map<int, vector<char>> *package_info = NULL;

    kcpev = kcpev_create_client(loop, "0", AF_INET);
    check(kcpev, "init client");

    ret = kcpev_connect(kcpev, server_ip, server_port);
    check(ret >= 0, "connect");

    kcpev_set_cb(kcpev, client_recv_cb, NULL);

    package_info = new unordered_map<int, vector<char>>;
    kcpev->data = package_info;

    evt = new ev_timer;
    evt->data = kcpev;
    ev_timer_init(evt, on_timer, 1, 0.01);
    ev_timer_start(loop, evt);

    return kcpev;

error:
    return NULL;
}

void on_stop(EV_P_ struct ev_timer *w, int revents)
{
    ev_break(EV_A_ EVBREAK_ALL);
}

TEST(KcpevTest, PackageTest)
{
    // 创建两个客户端，分别给服务端发送数据
    Kcpev *kcpev1 = create_client();
    EXPECT_NE(kcpev1, (Kcpev *)NULL);

	Kcpev *kcpev2 = create_client();
	EXPECT_NE(kcpev2, (Kcpev *)NULL);
 
    ev_timer evs;
    ev_timer_init(&evs, on_stop, 20, 0);
    ev_timer_start(kcpev1->loop, &evs);
   
    ev_run(kcpev1->loop, 0);

    for(const auto &pair: *static_cast<unordered_map<int, vector<char>> *>(kcpev1->data))
    {
        EXPECT_EQ(pair.second.size(), 0) << "package error found, key: " << pair.first;
    }

	for(const auto &pair: *static_cast<unordered_map<int, vector<char>> *>(kcpev2->data))
	{
		EXPECT_EQ(pair.second.size(), 0) << "package error found, key: " << pair.first;
	}
}

int main(int argc, char* argv[])
{
    if(argc != 3)
	{
		printf("usage:kcpev_remote_package_test server_ip server_port\n");
		return 0;
	}

	server_ip = argv[1];
	server_port = argv[2];

	int ac = 1;
    testing::InitGoogleTest(&ac, argv);
    return RUN_ALL_TESTS();
}

