#ifndef __M_CONSUMER_H__
#define __M_CONSUMER_H__

#include "../mqcommon/mq_logger.hpp"
#include "../mqcommon/mq_helper.hpp"
#include "../mqcommon/mq_msg.pb.h"
#include <iostream>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <vector>
#include <functional>

namespace bitmq {
    using ConsumerCallback = std::function<void(const std::string &qname, const BasicProperties *bp, const std::string)>;
    struct Consumer {
        using ptr = std::shared_ptr<Consumer>;
        std::string tag; // 消费者标签
        std::string qname;
        bool auto_ack;
        ConsumerCallback callback;

        Consumer(){
            DLOG("new Consumer: %p", this);
        }
        Consumer(const std::string &ctag, const std::string &queue_name, bool ack_flag, const ConsumerCallback &cb) :
        tag(ctag), qname(queue_name), auto_ack(ack_flag), callback(std::move(cb)) {
            DLOG("new Consumer: %p", this);
        }
        ~Consumer(){
            DLOG("delete Consumer: %p", this);
        }
    };

    class QueueConsumer
    {
    public:
        using ptr = std::shared_ptr<QueueConsumer>;
        QueueConsumer(const std::string &qname) : _qname(qname), _rr_seq(0)
        {
            DLOG("new QueueConsumer: %p", this);
        }
        Consumer::ptr create(const std::string &ctag, const std::string queue_name, bool ack_flag, const ConsumerCallback &cb)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            for (auto &consumer : _consumers)
            {
                if (consumer->tag == ctag)
                {
                    return Consumer::ptr();
                }
            }
            Consumer::ptr consumer = std::make_shared<Consumer>(ctag, queue_name, ack_flag, cb);
            _consumers.push_back(consumer);
            return consumer;
        }
        void remove(const std::string &ctag)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            for (auto it = _consumers.begin(); it != _consumers.end(); ++it)
            {
                if ((*it)->tag == ctag)
                {
                    _consumers.erase(it);
                    break;
                }
            }
            return;
        }
        bool empty()
        {
            std::unique_lock<std::mutex> lock(_mutex);
            return _consumers.size() == 0;
        }
        Consumer::ptr choose()
        {
            std::unique_lock<std::mutex> lock(_mutex);
            if (_consumers.empty())
            {
                return Consumer::ptr();
            }
            int idx = _rr_seq % _consumers.size();
            _rr_seq++;
            return _consumers[idx];
        }
        bool exists(const std::string &ctag)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            for (auto &consumer : _consumers)
            {
                if (consumer->tag == ctag)
                {
                    return true;
                }
            }
            return false;
        }
        void clear()
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _consumers.clear();
            _rr_seq = 0;
            return;
        }
        ~QueueConsumer()
        {
            DLOG("delete QueueConsumer: %p", this);
        }

    private:
        std::string _qname;
        std::mutex _mutex;
        uint64_t _rr_seq; // 轮询序号
        std::vector<Consumer::ptr> _consumers;
    };
    class ConsumerManager
    {
    public:
        using ptr = std::shared_ptr<ConsumerManager>;
        ConsumerManager() {}
        void initQueueConsumer(const std::string &qname)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _qconsumers.find(qname);
            if (it != _qconsumers.end())
            {
                return ;
            }
            auto qconsumer = std::make_shared<QueueConsumer>(qname);
            _qconsumers[qname] = qconsumer;
            return;
        }
        void destroyQueueConsumer(const std::string &qname)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _qconsumers.erase(qname);
        }
        Consumer::ptr create(const std::string &ctag, const std::string &queue_name, bool ack_flag, const ConsumerCallback &cb)
        {
            QueueConsumer::ptr qcp;
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _qconsumers.find(queue_name);
                if (it == _qconsumers.end())
                {
                    return Consumer::ptr();
                }
                qcp = it->second;
            }
            return qcp->create(ctag, queue_name, ack_flag, cb);
        }
        void remove(const std::string &ctag, const std::string &queue_name)
        {
            QueueConsumer::ptr qcp;
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _qconsumers.find(queue_name);
                if (it == _qconsumers.end())
                {
                    return ;
                }
                qcp = it->second;
            }
            qcp->remove(ctag);
            return;
        }
        Consumer::ptr choose(const std::string &queue_name)
        {
            QueueConsumer::ptr qcp;
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _qconsumers.find(queue_name);
                if (it == _qconsumers.end())
                {
                    return Consumer::ptr();
                }
                qcp = it->second;
            }
            return qcp->choose();
        }
        bool empty(const std::string &queue_name)
        {   
            QueueConsumer::ptr qcp;
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _qconsumers.find(queue_name);
                if (it == _qconsumers.end())
                {
                    DLOG("没有找到队列 %s 的消费者管理句柄！", queue_name.c_str());
                    return false;
                }
            }
            return qcp->empty();
        }
        bool exists(const std::string &ctag, const std::string &queue_name)
        {
            QueueConsumer::ptr qcp;
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _qconsumers.find(queue_name);
                if (it == _qconsumers.end())
                {
                    DLOG("没有找到队列 %s 的消费者管理句柄！", queue_name.c_str());
                    return false;
                }
                qcp = it->second;
            }
            return qcp->exists(ctag);
        }
        void clear()
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _qconsumers.clear();
            return;
        }
    private:
        std::mutex _mutex;
        std::unordered_map<std::string, QueueConsumer::ptr> _qconsumers;
    };
}

#endif