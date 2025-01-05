#include "Krpcapplication.h"
#include<cstdlib>
#include<unistd.h>

Krpcconfig KrpcApplication::m_config;// 全局变量
std::mutex KrpcApplication::m_mutex;
KrpcApplication* KrpcApplication::m_application = nullptr;//懒汉模式
 /* 读取命令行参数 初始化 
 */
 void KrpcApplication::Init(int argc,char **argv){
    if(argc<2){
        std::cout<<"format:command -i <configfile>"<<std::endl;
        exit(EXIT_FAILURE);
    }
    int o;
    std::string config_file;
    while(-1!= (o=getopt(argc,argv,"i:"))){//如果argv里面有i的话就代表指定了配置文件
        switch(o){
            case 'i':
                config_file=optarg;//将i后 的配置文件的路径赋给config_file
                break;
            case '?'://这个问号代表我们不喜欢出现其他参数比如上面除i以外的
                std::cout<<"format:command -i <configfile>"<<std::endl;
                exit(EXIT_FAILURE);
                break;
            case ':'://如果出现了-i，但是没有参数。
                std::cout<<"format:command -i <configfile>"<<std::endl;
                exit(EXIT_FAILURE);
                break;
            default:
                break;
        }
    }
    // 将服务器的ip和port存放到config中
    m_config.LoadConfigFile(config_file.c_str());
}

KrpcApplication &KrpcApplication::GetInstance(){
    std::lock_guard<std::mutex> lock(m_mutex);
    if(m_application==nullptr){
        m_application = new KrpcApplication(); 
        // atexit()函数可以将一个函数注册到程序的 “退出时执行队列” 中
        // 程序在退出的时候会尝试调用 “退出时执行队列” 的函数
        atexit(deleteInstance);//程序退出时调用deleteInstance,来销毁单例对象
    }
        
        return *m_application;
}
 void KrpcApplication::deleteInstance(){
    if(m_application){
        delete m_application;
    }
}
Krpcconfig& KrpcApplication::GetConfig(){
return m_config;
}