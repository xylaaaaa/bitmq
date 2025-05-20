#include "include/muduo/net/TcpClient.h"
#include "include/muduo/net/EventLoopThread.h"
#include "include/muduo/net/TcpConnection.h"
#include "include/muduo/base/CountDownLatch.h"
#include <iostream>
#include <functional>

class TranslateClient {
public:
    TranslateClient(const std::string &sip, int sport):_latch(1),
        _client(_loopthread.startLoop(), muduo::net::InetAddress(sip, sport), "TranslateClient"){
        _client.setConnectionCallback(std::bind(&TranslateClient::onConnection, this, std::placeholders::_1));
        _client.setMessageCallback(std::bind(&TranslateClient::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    }
    void connect()
    {
        _client.connect();
        _latch.wait();
    }
    bool send(const std::string& msg)
    {
        if (_conn->connected())
        { 
            _conn->send(msg);
            return true;
        }
        return false;
    }
private:
    //连接成功返回回调函数
    void onConnection(const muduo::net::TcpConnectionPtr &conn)
    {
        // 唤醒主线程的阻塞
        if (conn->connected())
        {
            _latch.countDown();
            _conn = conn;
        }
        else
        {
            _conn.reset();
        }
    }
    void onMessage(const muduo::net::TcpConnectionPtr &conn, muduo::net::Buffer *buf, muduo::Timestamp)
    {
        std::cout << "翻译结果" << buf->retrieveAllAsString() << std::endl;
         
    }
private: muduo::CountDownLatch _latch;
    muduo::net::EventLoopThread _loopthread;
    muduo::net::TcpClient _client;
    muduo::net::TcpConnectionPtr _conn;
};

int main()
{
    TranslateClient client("127.0.0.1", 8085);
    client.connect();
    
    while (1)
    {
        std::string buf;
        std::cin >> buf;
        client.send(buf);
    }
    return 0;
}