# kcpev
[![Build Status](https://travis-ci.org/disenone/kcpev.svg?branch=master)
](https://travis-ci.org/disenone/kcpev)

==========
设计：
TCP 和 UDP 同时使用，结构参考[同现有 TCP 服务器整合](https://github.com/skywind3000/kcp/wiki/Cooperate-With-Tcp-Server)

==========
###Feature:
* tcp 和 udp 收发，默认使用 udp；udp 不可用时切换为 tcp，也可以强行使用 tcp
* tcp 包使用 ringbuf 来重组，保证完整
* udp 使用 kcp 来保证收发可靠
* 收发正确性测试
	- tests/kcpev_package_test
	- tests/echo_server + tests/kcpev_remote_package_test
* 压测
	- pressure_test.sh

######
###TODO:
* udp 心跳包
* 断线重连

######
###Dependency:
* [libev](http://software.schmorp.de/pkg/libev.html)
* libuuid: apt-get install uuid-dev 
* [uthash](https://github.com/troydhanson/uthash)
* [googletest](https://github.com/google/googletest.git)

######
###Build:
* apt-get install libev-dev uuid-dev
* git submodule init && git submodule update 来下载第三方库
* ./build_third_party.sh
* make

