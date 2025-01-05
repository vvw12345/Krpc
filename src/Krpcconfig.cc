#include "Krpcconfig.h"
#include "memory"
//加载配置文件
void Krpcconfig::LoadConfigFile(const char *config_file){
    /* 首先将配置文件扫一遍读取进来 从文件变成字符流*/

    // unique_ptr 智能指针 在生命期结束时不需要调用delete显式删除（需要提供一个自定义删除器）
    // decltype(&fclose)  &fclose获得fclose函数的地址  decltype(&fclose)获取这个指针的类型
    // 这是一个文件类型的指针  &fclose作为其删除器
    std::unique_ptr<FILE,decltype(&fclose)>pf(
        fopen(config_file,"r"),
        &fclose
    );
    if(pf==nullptr){
        exit(EXIT_FAILURE);   
    }

    /* 其次由于我们的配置文件里面都是对一些参数进行设置
       因此每次用fget()函数读一行 再find()定位到=和\n 分割前后两个参数
       最后将其载入config_map
    */ 

    char buf[1024];
    //使用pf.get()方法获取原始指针
    //fget()是C标准库的函数 用于从指定的文件流（FILE*）中读取一行字符 直到遇到换行符或读取满
    while(fgets(buf,1024,pf.get())!=nullptr){
        std::string read_buf(buf);
        Trim(read_buf);//去掉字符串前后的空格
        if(read_buf[0]=='#'||read_buf.empty())continue;
        int index=read_buf.find('=');
        if(index==-1)continue;
        std::string key=read_buf.substr(0,index);
        Trim(key);//去掉key前后的空格
        int endindex=read_buf.find('\n',index);//找到行尾
        std::string value=read_buf.substr(index+1,endindex-index-1);//找到value，-1目的是排除\n
        Trim(value);//去掉value前后的空格
        config_map.insert({key,value}); // key代表的是参数名称 value代表的是参数值 分别是ip和port
    }
}
//查找key对应的value
std::string Krpcconfig::Load(const std::string &key){
    auto it=config_map.find(key);
    if(it==config_map.end()){
        return "";
    }
    return it->second;
}
//去掉字符串前后的空格
void Krpcconfig::Trim(std::string &read_buf){
    int index=read_buf.find_first_not_of(' ');//去掉字符串前的空格
    if(index!=-1){
        read_buf=read_buf.substr(index,read_buf.size()-index);
    }
      index=read_buf.find_last_not_of(' ');//去掉字符串后的空格
    if(index!=-1){
        read_buf=read_buf.substr(0,index+1);
    }
}