#include <kcpev.h>
#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <dbg.h>
#include <unistd.h>
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

void stdin_read(EV_P_ struct ev_io *w, int revents)
{
    static int odd = 0;
	char buf[KCPEV_BUFFER_SIZE];
	char *buf_in;
    int ret = -1;
    Kcpev *kcpev = NULL;

	buf_in = fgets(buf, sizeof(buf), stdin);
	check(buf_in != NULL, "get stdin");

    kcpev = (Kcpev *)w->data;
	//odd = (odd + 1) % 2;
	if(odd)
	    ret = kcpev_send(kcpev, buf, strlen(buf));
	else
		ret = kcpev_send_tcp(kcpev, buf, strlen(buf));

	check(ret >= 0, "");

	printf(">> ");
	fflush(stdout);
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

	ev_io ev_stdin;
	ev_stdin.data = kcpev;
	ev_io_init(&ev_stdin, stdin_read, STDIN_FILENO, EV_READ);
	ev_io_start(kcpev->loop, &ev_stdin);

    ev_run(kcpev->loop, 0);
    return 0;
}

