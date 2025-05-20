#include "include/muduo/net/TcpServer.h"
#include "include/muduo/net/EventLoop.h"
#include "include/muduo/net/TcpConnection.h"
#include <iostream>
#include <functional>
#include <unordered_map>
class TranslateServer {
public:
    TranslateServer(int port):_server(&_baseloop, 
        muduo::net::InetAddress("0.0.0.0", port),
        "TranslateServer", muduo::net::TcpServer::kReusePort)
    {
        // 将类函数设置为服务器的回调函数
        // 函数适配器, bind, 对指定的函数进行参数绑定
        auto func1 = bind(&TranslateServer::onConnection, this, std::placeholders::_1);
        _server.setConnectionCallback(func1);
        auto func2 = bind(&TranslateServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
        _server.setMessageCallback(func2);
    }
    
    void start()
    {
        _server.start();//开始事件监听
        _baseloop.loop();//开始事件监控,是一个阻塞接口
    }

private:
    void onConnection(const muduo::net::TcpConnectionPtr& conn)
    {
        //新连接建立成功时的回调函数
        if (conn->connected() == true)
        {
            std::cout << "新连接建立成功" << std::endl;
        }
        else
        {
            std::cout << "新连接关闭" << std::endl;
        }

    }
    std::string translate(const std::string &str)
    {
        static std::unordered_map<std::string, std::string> dict_map = {
            {"hello", "你好"}
        };
        auto it = dict_map.find(str);
        if (it == dict_map.end())
            return "没有";
        return it->second;
    }
    void onMessage(const muduo::net::TcpConnectionPtr &conn, muduo::net::Buffer *buf, muduo::Timestamp)
    {
        //收到请求时的回调函数
        //1.从buff中取数据
        std::string str = buf->retrieveAllAsString();
        //2.调用translate
        std::string resp = translate(str);
        //3.对客户端进行响应结果
        conn->send(resp); 
    }
    muduo::net::EventLoop _baseloop;
    muduo::net::TcpServer _server;
};

int main()
{
    TranslateServer server(8085);
    server.start();
    return 0;
}