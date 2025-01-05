# Krpc

Remote Procedure call库的简单实现

原项目：https://github.com/youngyangyang04/Krpc

# 理论部分

## 信息的编解码

[如果面试提到Protobuf，面试官问其原理怎么办？ - 知乎](https://zhuanlan.zhihu.com/p/633656133)

client和server可能是用不同语言编写的，你的编解码方案必须通用且不能和语言绑定

> 纯文本方式——JSON语言   对于数据较少的场合（如服务器和客户端的交互）效果很好  
>
> 但服务器和服务器之间往往需要传递大量数据，纯文本方式效率很低，此时传递二进制数据更好——protobuf

### Protobuf需要传递的三种信息

Protobuf协议需要传递以下三种信息：字段类型，字段名称，字段值

> 字段类型可以直接编码
>
> **字段名称不需要传递**  因为通信双方都知道有什么字段，只需要完成字段值和字段名称的匹配即可……因此**给每一个字段名称赋予一个唯一的识别号**

**字段值**

```markdown
示例：id 43
<id>43</id>   //XML
{"id":43}     //JSON

// Protobuf
// 消息定义
message Msg {
  optional int32 id = 1;
}

// 实例化
Msg msg;
msg.set_id(43);
// 最终编码为
082b  
```

**第一个比特位如果是1那么表示接下来的一个比特依然要用来解释为一个数字，如果第一个比特为0，那么说明接下来的一个字节不是用来表示该数字的。**

**既然无符号数字可以方便的进行变长编码，那么我们将有符号数字映射称为无符号数字不就可以了**，这就是所谓的ZigZag编码。

```markdown
原始信息      编码后
0            0 
-1           1 
1            2
-2           3
2            4
-3           5
3            6

...          ...

2147483647   4294967294
-2147483648  4294967295
```



### Protobuf编码

其中value比较简单，也就是字段值；而**字段名称和字段类型会被拼接成key**，protobuf中共有6种类型，因此只需要3个比特位即可；字段名称只需要存储对应的编号，这样可以就可以这样编码：

```text
(字段编号 << 3) | 字段类型
```

![img](https://pic3.zhimg.com/v2-67fe0d297eb656368ed4d3984180366e_1440w.png)



### 示例

```protobuf
// 指定 Protobuf 版本为版本 3（最新版本）
syntax = "proto3";
 
// 指定protobuf包名，防止类名重复
package com.iot.protobuf;
 
// 生成的文件存放在哪个包下，对于新手来说，不指定生成到哪个目录下即可，不建议指定包（否则，可能让你怀疑人生）
// option c_package = "com.iot.protos";
 
// 生成的类名，如果没有指定，会根据文件名自动转驼峰来命名
option c_outer_classname = "StudentProto";
 
// 定义了一个Student类
message Student {
    // 后面的值（1、2、3、4等）作为序列化后的二进制编码中的字段的唯一标签
    // 因为 1-15比 16 会少一个字节，所以尽量使用 1-15 来指定常用字段。
    int32 id = 1;
    string name = 2;
    string email = 3;
    string address = 4;
}

```



## Glog

支持四种日志级别：Info,Warning,Error,Fatal

```c
FLAGS_minloglevel = 1; // 只输出WARNING及以上级别的日志
FLAGS_log_dir = "/path/to/log_directory"; // 日志存放目录
FLAGS_max_log_size = 1024 // 单个日志最大大小
    
日志一开始是放在内存里面的 只有到一定大小之后才会放进磁盘
FLAGS_logbuflevel = -1; // 禁用日志级别缓冲
FLAGS_logbufsecs = 5;   // 设置写入磁盘的时间间隔为5秒
```



## ZooKeeper

一个分布式服务提供框架，`callee`将其提供的服务和自身信息发布在ZK上，`caller`根据需要的服务在ZK上调用服务。ZooKeeper本质和一个文件系统差不多，只不过其可以存储关联性的数据，这种特性使得其无法存储大量的数据，每个节点的数据上限为1MB。

<img src="https://i-blog.csdnimg.cn/blog_migrate/9823437522c142da592cfd48c57d0f5d.png" alt="请添加图片描述" style="zoom:40%;" />

> 永久性节点在断开连接后依然存在，临时性节点则会消失

**watcher机制就是ZooKeeper客户端对某个znode建立一个watcher事件，当该znode发生变化时，这些ZK客户端会收到ZK服务端的通知，然后ZK客户端根据znode的变化来做出业务上的改变。**





# 项目概述
本项目基于protobuf的c++分布式网络通信框架，使用了zookeeper作为服务中间件，负责解决在分布式服务部署中 服务的发布与调用、消息的序列与反序列化、网络包的收发等问题，使其能提供高并发的远程函数调用服务，可以让使用者专注于业务，快速实现微服务的分布式部署，项目会继续完善的欢迎，大家一起学习。
## 运行环境
    Ubuntu 20.04 LTS
## 编译指令
    mkdir build && cd build && cmake .. && make
    进入到example目录下，运行./server和./client，即可完成服务发布和调用。

## 库准备
    muduo,https://blog.csdn.net/QIANGWEIYUAN/article/details/89023980
    zookeeper:
    安装 Zookeeper,sudo apt install libzookeeper-mt-dev
    安装 Zookeeper 开发库,sudo apt install libzookeeper-mt-dev
    protoc，本地版本为3.12.4，ubuntu22使用sudo apt-get install protobuf-compiler libprotobuf-dev安装默认就是这个版本
    glog安装：sudo apt-get install libgoogle-glog-dev libgflags-dev
## 整体的框架
muduo库：负责数据流的网络通信，采用了多线程epoll模式的IO多路复用，让服务发布端接受服务调用端的连接请求，并由绑定的回调函数处理调用端的函数调用请求

protobuf：负责rpc方法的注册，数据的序列化和反序列化，相比于文本存储的xml和json来说，protobuf是二进制存储，且不需要存储额外的信息，效率更高

zookeeper：负责分布式环境的服务注册，记录服务所在的ip地址以及port端口号，可动态地为调用端提供目标服务所在发布端的ip地址与端口号，方便服务所在ip地址变动的及时更新

TCP沾包问题处理：定义服务发布端和调用端之间的消息传输格式，记录方法名和参数长度，防止沾包。

后续增加了glog的日志库，进行异步的日志记录。

## 性能测试
在Kclient中进行了手写了一个简单的测试，可以作为一个性能参考，目前还不是最优还在继续优化。