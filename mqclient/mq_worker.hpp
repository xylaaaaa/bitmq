#ifndef BITMQ_MQCLIENT_MQ_WORKER_HPP
#define BITMQ_MQCLIENT_MQ_WORKER_HPP

#include "muduo/net/EventLoopThread.h"
#include "../mqcommon/mq_logger.hpp"
#include "../mqcommon/mq_helper.hpp"
#include "../mqcommon/mq_threadpool.hpp"

namespace bitmq {
    class AsyncWorker {
        public: 
            using ptr = std::shared_ptr<AsyncWorker>;
            muduo::net::EventLoopThread loopthread;
            threadpool pool;
    };
}

#endif
