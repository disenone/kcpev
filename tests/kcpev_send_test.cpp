#include <kcpev.h>
#include <iostream>
#ifdef _WIN32
#   include <winsock2.h>
#   include <WS2tcpip.h>
#   include <stdint.h>
#else
#   include <netdb.h>
#   include <sys/types.h>
#   include <sys/socket.h>
#   include <unistd.h>
#endif
#include <dbg.h>
#include <string>
#include <fcntl.h>
#include "test.h"

using namespace std;

void client_recv_cb(Kcpev* kcpev, const char* buf, size_t len)
{
    char *data = new char[len + 1];
    memcpy(data, buf, len);
    data[len] = '\0';
    debug("%s", data);
	printf(">> ");
	fflush(stdout);
    delete data;
}

void on_stdin_read(EV_P_ struct ev_watcher *w, int revents, const char *buf, size_t len)
{
    static int odd = 0;
    int ret = -1;
	//odd = (odd + 1) % 2;
    Kcpev *kcpev = (Kcpev *)w->data;
	if(odd)
	    ret = kcpev_send(kcpev, buf, strlen(buf));
	else
		ret = kcpev_send_tcp(kcpev, buf, strlen(buf));

	check(ret >= 0, "");
error:
    return;
}

Kcpev* create_client()
{
    Kcpev *kcpev = NULL;
	struct ev_loop *loop = EV_DEFAULT;
    int ret = 0;

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

int main(int argc, char* argv[])
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
        return 0;
    }   

    sleep(1);

    // parent
    child_pid = pid;

    Kcpev *kcpev = create_client();

    setup_stdin(kcpev->loop, kcpev, on_stdin_read);

    ev_run(kcpev->loop, 0);
    return 0;
}

