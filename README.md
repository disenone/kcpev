# kcpev
![build_status](https://travis-ci.org/disenone/kcpev.svg?branch=master)
[![Build Status](https://travis-ci.org/disenone/kcpev.svg?branch=master)
](https://travis-ci.org/disenone/kcpev)

#######
设计：
TCP 和 UDP 同时使用，结构参考[同现有 TCP 服务器整合](https://github.com/skywind3000/kcp/wiki/Cooperate-With-Tcp-Server)

Dependency:
* [libev](http://software.schmorp.de/pkg/libev.html)
* libuuid: apt-get install uuid-dev 
* [uthash](https://github.com/troydhanson/uthash)

TODO:
* 收发正确性测试
	- tests/kcpev_package_test
	- tests/echo_server + tests/kcpev_remote_package_test
* 压测
	- pressure_test.sh
* udp 心跳包
* 断线重连

Build:
* install libev uuid-dev
* git submodule init && git submodule update 来下载第三方库
* build googletest:
	- cd third_party/googletest
	- mkdir build && cd build
	- cmake ..
	- make
* 在 kcpev 根目录下 make

