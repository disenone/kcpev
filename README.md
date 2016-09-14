# kcpev
[![Build Status](https://travis-ci.org/disenone/kcpev.svg?branch=master)
](https://travis-ci.org/disenone/kcpev)

设计：
TCP 和 UDP 同时使用，结构参考[同现有 TCP 服务器整合](https://github.com/skywind3000/kcp/wiki/Cooperate-With-Tcp-Server)

==========
###Feature:
* Linux 和 Windows 下都能编译运行，但 Windows 下只支持客户端的允许，服务端 udp 不能正确接收数据
* tcp 和 udp 收发，默认使用 udp；udp 不可用时切换为 tcp，也可以强行使用 tcp
* tcp 包使用 ringbuf 来重组，保证完整
* udp 使用 kcp 来保证收发可靠
* 收发正确性测试
	- tests/kcpev_package_test
	- tests/echo_server + tests/kcpev_remote_package_test
* 压测
	- pressure_test.sh

==========
###TODO:
* conv 有可能为0的问题
* udp 心跳包
* 断线重连

==========
###Dependency:
* [libev](http://software.schmorp.de/pkg/libev.html)
* [libuuid](https://github.com/karelzak/util-linux/tree/master/libuuid)
* [uthash](https://github.com/troydhanson/uthash)
* [googletest](https://github.com/google/googletest.git)

==========
###Build:
* Linux
    - apt-get install libev-dev uuid-dev
    - ./travis.sh

* Windows
    - install cmake
    - powershell
    - ./build.ps1

* Mac
    - brew install libev
    - ./travis.sh
