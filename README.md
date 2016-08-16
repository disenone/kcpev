# kcpev

#######
设计：
TCP 和 UDP 同时使用，结构参考[同现有 TCP服务器整合](https://github.com/skywind3000/kcp/wiki/Cooperate-With-Tcp-Server)

Dependency:
* [libev](http://software.schmorp.de/pkg/libev.html)
* libuuid: apt-get install uuid-dev 
* [uthash](https://github.com/troydhanson/uthash)

TODO:
* 收发正确性测试：统计收包和发包的数量
* 压测
* udp 心跳包
* 断线重连

Build:
* git submodule init && git submodule update 来下载第三方库
* build googletest:
	- cd third_party/googletest
	- mkdir build && cd build
	- cmake ..
	- make
* 在 kcpev 根目录下 make
