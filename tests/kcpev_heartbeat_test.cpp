#include <kcpev.h>
#include <stdio.h>
#ifdef _WIN32
#   include <winsock2.h>
#   include <WS2tcpip.h>
#   include <stdint.h>
#else
#   include <netdb.h>
#   include <sys/types.h>
#   include <sys/socket.h>
#endif
#include <dbg.h>
#include <gtest/gtest.h>
#include <string>
#include <fcntl.h>
#include "test.h"

using namespace std;

void on_stop(EV_P_ struct ev_timer *w, int revents)
{
    ev_break(EV_A_ EVBREAK_ALL);
}

void on_stop_heartbeat(EV_P_ struct ev_timer *w, int revents)
{
	Kcpev *client = (Kcpev *)w->data;
	
    ev_timer_stop(client->loop, client->udp.evh);
}

void client_recv_cb(Kcpev* kcpev, const char* buf, size_t len)
{
}

Kcpev* create_client()
{
    Kcpev *kcpev = NULL;
	struct ev_loop *loop = EV_DEFAULT;
    int ret = 0;
    ev_timer *evt = NULL;

    kcpev = kcpev_create_client(loop, "0", AF_INET);
    check(kcpev, "init client");

    ret = kcpev_connect(kcpev, "127.0.0.1", "33333");
    check(ret >= 0, "connect");

    kcpev_set_cb(kcpev, client_recv_cb, NULL);

    return kcpev;

error:
    return NULL;
}


std::string get_server_full_path_name()
{
    string exe_path(1024 * 10, '\0');

    readlink("/proc/self/exe", &exe_path[0], exe_path.capacity());

    int pos = exe_path.find_last_of('/');

    string server_path = exe_path.substr(0, pos + 1);

    return server_path;
}

TEST(KcpevTest, PackageTest)
{
    signal(SIGCHLD, SIG_IGN);

    const std::string server_full_name = get_server_full_path_name() + "kcpev_echo_server_test";

    pid_t child_pid = 0;
    // fork a child process for create a server.
    pid_t pid = fork();
    if (pid == 0) // child exec server
    {   
        int ret_exec = execl(server_full_name.c_str(), "kcpev_echo_server_test", NULL);
        if (ret_exec < 0)
            std::cerr << "execl error with errno: " << errno << " " << strerror(errno) << std::endl;
        return;
    }   

    sleep(1);

    // parent
    child_pid = pid;

    Kcpev *client = create_client();
    EXPECT_NE(client, (Kcpev *)NULL);
 
    ev_timer evs;
    ev_timer_init(&evs, on_stop, 60, 0);
    ev_timer_start(client->loop, &evs);
  
    ev_timer evh;
    ev_timer_init(&evh, on_stop_heartbeat, 15, 0);
    ev_timer_start(client->loop, &evh);
	evh.data = client;
  
    ev_run(client->loop, 0);

    EXPECT_EQ(client->udp.status, UDP_INVALID);

    int ret_kill = ::kill(child_pid, SIGINT);
    if (ret_kill < 0)
        std::cerr << "kill error with errno: " << errno << " " << strerror(errno) << std::endl;
}

int main(int argc, char* argv[])
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

