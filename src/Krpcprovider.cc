#include "Krpcprovider.h"
#include "Krpcapplication.h"
#include "Krpcheader.pb.h"
#include <iostream>

/// 这个函数用于注册服务对象和其对应的 RPC 方法，以便服务端处理客户端的请求。
/* 首先读出该服务的名称service.name
   其次读出对应的方法service.method  可能有多个 将其同一保存到一个hash表里面去
   最后用ervice.name和这个hash表对应起来
*/
void KrpcProvider::NotifyService(google::protobuf::Service *service)
{

    // 服务端需要知道对方想要调用的具体服务对象和方法，
    // 这些信息会保存在一个数据结构（如 ServiceInfo）中。
    ServiceInfo service_info;

    // 参数类型设置为 google::protobuf::Service，是因为所有由 protobuf 生成的服务类
    // 都继承自 google::protobuf::Service，这样我们可以通过基类指针指向子类对象，
    // 实现动态多态。

    // 通过动态多态调用 service->GetDescriptor()，
    // GetDescriptor() 方法会返回 protobuf 生成的服务类的描述信息（ServiceDescriptor）。
    const google::protobuf::ServiceDescriptor *psd = service->GetDescriptor();

    // 通过 ServiceDescriptor，我们可以获取该服务类中定义的方法列表，
    // 并进行相应的注册和管理。

    // 获取服务的名字
    std::string service_name = psd->name();
    // 获取服务端对象service的方法数量
    int method_count = psd->method_count();

    // 打印service_name
    std::cout << "service_name=" << service_name << std::endl;

    for (int i = 0; i < method_count; ++i)
    {
        // 获取服务中的方法描述
        const google::protobuf::MethodDescriptor *pmd = psd->method(i);
        std::string method_name = pmd->name();
        std::cout << "method_name=" << method_name << std::endl;
        service_info.method_map.emplace(method_name, pmd);
    }
    service_info.service = service;
    service_map.emplace(service_name, service_info);
}

// 启动rpc服务节点，开始提供rpc远程网络调用服务
void KrpcProvider::Run()
{
    // 读取配置文件rpcserver的信息
    std::string ip = KrpcApplication::GetInstance().GetConfig().Load("rpcserverip");
    int port = atoi(KrpcApplication::GetInstance().GetConfig().Load("rpcserverport").c_str());
    // 使用muduo网络库，创建address对象
    muduo::net::InetAddress address(ip, port);

    // 创建tcpserver
    std::shared_ptr<muduo::net::TcpServer> server = std::make_shared<muduo::net::TcpServer>(&event_loop, address, "KrpcProvider");

    // 绑定连接回调和消息会调，分离了网络连接业务和消息处理业务
    // 连接回调是在网络连接成功/失败 触发的回调函数
    // 消息回调是在事件发生之前或之后，向客户的应用服务器发送请求，应用服务器可以据此进行必要的数据同步或根据业务需求干预事件的后续处理流程
    server->setConnectionCallback(std::bind(&KrpcProvider::OnConnection, this, std::placeholders::_1));
    /* std::bind用于创建一个新的可调用对象，这里它将KrpcProvider类的成员函数OnConnection绑定到当前对象（this）上。
       std::placeholders::_1是一个占位符，表示回调函数的第一个参数。当连接回调函数被调用时，这个占位符将被实际的连接事件参数替换。
    */
    server->setMessageCallback(std::bind(&KrpcProvider::OnMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    // 设置muduo库的线程数量
    server->setThreadNum(4);

    // 把当然rpc节点上要发布的服务全部注册到zk上面，让rpc client客户端可以在zk上发现服务
    ZkClient zkclient;
    zkclient.Start();
    // service_name为永久节点，method_name为临时节点
    /*
        前面的KrpcProvider::NotifyService方法已经将service_name和method_name都存在hash表里面了
        这里通过遍历hash表，在ZK上将所有方法放进去
    */
    for (auto &sp : service_map)
    {
        // service_name 在zk中的目录下是"/"+service_name
        std::string service_path = "/" + sp.first;
        zkclient.Create(service_path.c_str(), nullptr, 0);
        for (auto &mp : sp.second.method_map) // 这里的method_map是service_name对应的method_name哈希表
        {
            std::string method_path = service_path + "/" + mp.first; // 每一个方法也有一个目录 目录里面应该存放方法对应的数据(如ip和port)
            char method_path_data[128] = {0};
            sprintf(method_path_data, "%s:%d", ip.c_str(), port); // 打印ip和端口
            // ZOO_EPHEMERAL表示这个节点是临时节点，在客户端断开连接后，zk会自动删除这个节点
            zkclient.Create(method_path.c_str(), method_path_data, strlen(method_path_data), ZOO_EPHEMERAL);
        }
    }
    // rpc服务端准备启动，打印信息   这里的ip和port是服务器的ip和端口
    std::cout << "RpcProvider start service at ip:" << ip << " port:" << port << std::endl;
    // 启动网络服务
    server->start();
    event_loop.loop();
}

// 连接回调
void KrpcProvider::OnConnection(const muduo::net::TcpConnectionPtr &conn)
{
    if (!conn->connected())
    { // 如果连接关闭则断开连接即可。
        conn->shutdown();
    }
}
// 消息回调
// 在框架内部,RpcProvider和Rpcconsumer之间协商好通信使用的的protobuf的数据类型。
// 已建立连接用户的读写事件回调 如果远程有rpc服务的调用请求，那么OnMessage方法就会响应。
// 一般的主要格式是header_size(4个字节)+header_str+arg_str 一般来说header_str是服务对象和服务对象中的方法，arg服务器对象方法设置的参数。

void KrpcProvider::OnMessage(const muduo::net::TcpConnectionPtr &conn, muduo::net::Buffer *buffer, muduo::Timestamp receive_time)
{
    std::cout<<"OnMessage"<<std::endl;
    // 网络上接收远程rpc调用请求的字符流
    std::string recv_buf = buffer->retrieveAllAsString();

    // 从字符流中读取前4个字节的内容。
    uint32_t header_size = 0;
    recv_buf.copy((char *)&header_size, 4, 0);
    // 根据hender_size读取到数据头原始字符流，反序列化数据，得到rpc的详细请求。
    std::string rpc_header_str = recv_buf.substr(4, header_size); // 获取rpc头数据
    Krpc::RpcHeader rpcheader;
    std::string service_name;
    std::string method_name;
    uint32_t args_size;
    if (rpcheader.ParseFromString(rpc_header_str)) // 解析头数据对其进行反序列化
    {
        // 反序列化头部成功
        service_name = rpcheader.service_name();
        method_name = rpcheader.method_name();
        args_size = rpcheader.args_size();
    }
    else
    {
        // 反序列化失败
        std::cout << "rpc_header_str" << rpc_header_str << "parse error!" << std::endl;
        return;
    }
    // 获取rpc方法参数的字符流数据
    std::string args_str = recv_buf.substr(4 + header_size, args_size);
    // 打印调试信息
    std::cout << "============================================" << std::endl;
    std::cout << "header_size: " << header_size << std::endl;
    std::cout << "rpc_header_str: " << rpc_header_str << std::endl;
    std::cout << "service_name: " << service_name << std::endl;
    std::cout << "method_name: " << method_name << std::endl;
    std::cout << "args_str: " << args_str << std::endl;
    std::cout << "============================================" << std::endl;

    // 获取service对象和method对象
    auto it = service_map.find(service_name); // 消息反序列化得到的service_name和ZK上已经注册的service_name比对一下
    if (it == service_map.end())              // 如果有则调取对应的服务
    {
        std::cout << service_name << "is not exist!" << std::endl;
        return;
    }
    auto mit = it->second.method_map.find(method_name);
    if (mit == it->second.method_map.end())
    {
        std::cout << service_name << "." << method_name << "is not exist!" << std::endl;
        return;
    }
    google::protobuf::Service *service = it->second.service;        // 获取服务对象
    const google::protobuf::MethodDescriptor *method = mit->second; // 获取方法对象

    // 生成rpc方法调用请求的request和响应的response参数.
    google::protobuf::Message *request = service->GetRequestPrototype(method).New(); // 通过 GetRequestPrototype，可以根据方法描述符动态获取对应的请求消息类型，并New（）实例化该类型的对象。
    if (!request->ParseFromString(args_str))
    {
        std::cout << service_name << "." << method_name << "parse error!" << std::endl;
        return;
    }
    google::protobuf::Message *response = service->GetResponsePrototype(method).New();

    // 给下面的mehod方法的调用，绑定一共Closure的回到函数
    google::protobuf::Closure *done = google::protobuf::NewCallback<KrpcProvider,
                                                                    const muduo::net::TcpConnectionPtr &,
                                                                    google::protobuf::Message *>(this,
                                                                                                 &KrpcProvider::SendRpcResponse,
                                                                                                 conn, response);

    // 在框架上根据远端rpc请求，调用当前rpc节点上发布的方法
    // new UserService().Login(controller, request, response, done)
    service->CallMethod(method, nullptr, request, response, done); // request,response 是method方法(如login)的参数。done是执行完method方法后会执行的回调函数。
}

// 通过 GetRequestPrototype，可以根据方法描述符动态获取对应的请求消息类型，并实例化该类型的对象。
void KrpcProvider::SendRpcResponse(const muduo::net::TcpConnectionPtr &conn, google::protobuf::Message *response)
{
    std::string response_str;
    if (response->SerializeToString(&response_str)) //SerializeToString是Protobuf提供的消息序列化方法 这里调用将response消息序列化
    {
        // 序列化成功，通过网络把rpc方法执行的结果返回给rpc的调用方
        conn->send(response_str);
    }
    else
    {
        std::cout << "serialize error!" << std::endl;
    }
    conn->shutdown(); // 模拟http短链接，由rpcprovider主动断开连接
}
KrpcProvider::~KrpcProvider(){
    std::cout<<"~KrpcProvider()"<<std::endl;
    event_loop.quit();
}
