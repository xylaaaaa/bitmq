#ifndef BITMQ_MQSERVER_MQ_MESSAGE_HPP
#define BITMQ_MQSERVER_MQ_MESSAGE_HPP

#include "../mqcommon/mq_logger.hpp"
#include "../mqcommon/mq_helper.hpp"
#include "../mqcommon/mq_msg.pb.h"
#include <iostream>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <list>

namespace bitmq {
    #define DATAFILE_SUBFIX ".mqd"
    #define TMPFILE_SUBFIX ".mqd.tmp"
    using MessagePtr = std::shared_ptr<bitmq::Message>;
    class MessageMapper {
        public:
            MessageMapper(std::string &basedir, const std::string &qname):
            _qname(qname) {
                if (basedir.back() != '/') basedir.push_back('/');
                _datafile = basedir + qname + DATAFILE_SUBFIX;
                _tmpfile = basedir + qname + TMPFILE_SUBFIX;
                if (FileHelper(basedir).exists() == false)
                {
                    assert(FileHelper::createDirectory(basedir));
                }
                createMsgFile();
            }
            bool createMsgFile()
            {
                if (FileHelper(_datafile).exists() == true)
                {
                    return true;
                }
                bool ret = FileHelper::createFile(_datafile);
                if (ret == false)
                {
                    DLOG("创建队列数据文件 %s 失败!", _datafile.c_str());
                    return false;
                }
                return true;
            }
            
            void removeMsgFile()
            {
                FileHelper::removeFile(_datafile);
                FileHelper::removeFile(_tmpfile);
            }

            bool insert(MessagePtr &msg)
            {
                return insert(_datafile, msg);
            }

            bool remove(MessagePtr &msg)
            {
                msg->mutable_payload()->set_valid("0");
                std::string body = msg->payload().SerializeAsString();   
                if (body.size() != msg->length())
                {
                    DLOG("不能修改文件中的数据信息，因为新生成的数据与原数据长度不一致!");
                    return false;
                }
                FileHelper helper(_datafile);
                bool ret = helper.write(body.c_str(), msg->offset(), body.size());
                if (ret == false)
                {
                    DLOG("向队列数据文件写入数据失败！");
                    return false;
                }
                return true;
            }
            std::list<MessagePtr> gc() 
            {
                bool ret;
                std::list<MessagePtr> result;
                ret = load(result);
                if (ret == false)
                {
                    DLOG("加载有效数据失败！\n");
                    return result;
                }
                FileHelper::createFile(_tmpfile);
                for (auto &msg : result)
                {
                    DLOG("向临时文件写入数据: %s", msg->payload().body().c_str());
                    ret = insert(_tmpfile, msg);
                    if (ret == false)
                    {
                        DLOG("向临时文件写入消息数据失败！！");
                        return result;
                    }
                }
                ret = FileHelper::removeFile(_datafile);
                if (ret == false)
                {
                    DLOG("删除源文件失败！");
                    return result;
                }
                ret = FileHelper(_tmpfile).rename(_datafile);
                if (ret == false)
                {
                    DLOG("修改临时文件失败");
                    return result;
                }
                return result;
            }

        private:
            bool load(std::list<MessagePtr> &result)
            {
                FileHelper data_file_helper(_datafile);
                size_t offset = 0, msg_size;
                size_t fsize = data_file_helper.size();
                bool ret;
                while(offset < fsize)
                {
                    ret = data_file_helper.read((char*)&msg_size, offset, sizeof(size_t));
                    if (ret == false)
                    {
                        DLOG("读取消息长度失败！");
                        return false;
                    }
                    offset += sizeof(size_t);
                    std::string msg_body(msg_size, '\0');
                    data_file_helper.read(&msg_body[0], offset, msg_size);
                    if (ret == false)
                    {
                        DLOG("读取消息数据失败！");
                        return false;
                    }
                    offset += msg_size;
                    MessagePtr msgp = std::make_shared<Message>();
                    msgp->mutable_payload()->ParseFromString(msg_body); // 反序列化

                    if (msgp->payload().valid() == "0")
                    {
                        DLOG("加载到无效消息：%s", msgp->payload().body().c_str());
                        continue;
                    }
                    result.push_back(msgp);
                }
                return true;
            }

            bool insert(const std::string &filename, MessagePtr &msg)
            {
                // 消息的序列化
                std::string body = msg->payload().SerializeAsString();
                FileHelper helper(filename);
                size_t fsize = helper.size();
                size_t msg_size = body.size();
                // 先写入8字节数据长度
                bool ret = helper.write((char*)&msg_size, fsize, sizeof(size_t));
                if (ret == false)
                {
                    DLOG("向队列数据文件写入数据长度失败! ");
                    return false;
                }
                ret = helper.write(body.c_str(), fsize + sizeof(size_t), body.size());
                if (ret == false)
                {
                    DLOG("向队列数据文件写入失败 ");
                    return false;
                }
                msg->set_offset(fsize + sizeof(size_t));
                msg->set_length(body.size());
                return true;
            }

            std::string _qname;
            std::string _datafile;
            std::string _tmpfile;
    };

    class QueueMessage{
        public:
            using ptr = std::shared_ptr<QueueMessage>;
            QueueMessage(std::string &basedir, const std::string &qname):_mapper(basedir, qname),
                _qname(qname), _valid_count(0), _total_count(0) {}

            bool recovery()
            {
                // 修复历史消息
                std::unique_lock<std::mutex> lock(_mutex);
                _msgs = _mapper.gc();
                for (auto &msg : _msgs)
                {
                    _durable_msgs.insert(std::make_pair(msg->payload().properties().id(), msg));
                }
                _valid_count = _total_count = _msgs.size();
                return true;
            }
            bool insert(const BasicProperties *bp, const std::string &body, bool queue_is_durable)
            {
                MessagePtr msg = std::make_shared<Message>();
                msg->mutable_payload()->set_body(body);
                if (bp != nullptr)
                {
                    DeliveryMode mode = queue_is_durable ? bp->delivery_mode() : DeliveryMode::UNDURABLE;
                    msg->mutable_payload()->mutable_properties()->set_id(bp->id());
                    msg->mutable_payload()->mutable_properties()->set_delivery_mode(mode);
                    msg->mutable_payload()->mutable_properties()->set_routing_key(bp->routing_key());
                }
                else
                {
                    DeliveryMode mode = queue_is_durable ? DeliveryMode::DURABLE : DeliveryMode::UNDURABLE;
                    msg->mutable_payload()->mutable_properties()->set_id(UUIDHelper::uuid());
                    msg->mutable_payload()->mutable_properties()->set_delivery_mode(mode);
                    msg->mutable_payload()->mutable_properties()->set_routing_key("");
                }
                std::unique_lock<std::mutex> lock(_mutex);
                if (msg->payload().properties().delivery_mode() == DeliveryMode::DURABLE)
                {
                    msg->mutable_payload()->set_valid("1");
                    bool ret = _mapper.insert(msg);
                    if (ret == false)
                    {
                        DLOG("持久化存储消息: %s 失败了", body.c_str());
                        return false;
                    }
                    _valid_count += 1;
                    _total_count += 1;
                    _durable_msgs.insert(std::make_pair(msg->payload().properties().id(), msg));

                }
                // 内存的管理
                _msgs.push_back(msg);
                return true;
            }
            MessagePtr front() 
            {
                std::unique_lock<std::mutex> lock(_mutex);
                if (_msgs.size() == 0)
                {
                    return MessagePtr();
                }
                MessagePtr msg = _msgs.front();
                _msgs.pop_front();

                _waitack_msgs.insert(std::make_pair(msg->payload().properties().id(), msg));
                return msg;
            }

            bool remove(const std::string &msg_id)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _waitack_msgs.find(msg_id);
                if (it == _waitack_msgs.end())
                {
                    DLOG("没有找到要删除的消息:%s", msg_id.c_str());
                    return true;
                }
                if (it->second->payload().properties().delivery_mode() == DeliveryMode::DURABLE)
                {
                    _mapper.remove(it->second);
                    _durable_msgs.erase(msg_id);
                    _valid_count -= 1;
                    gc();
                }
                _waitack_msgs.erase(msg_id);
                return true;
            }
            size_t getable_count()
            {
                std::unique_lock<std::mutex> lock(_mutex);
                return _msgs.size();
            }
            size_t total_count()
            {
                std::unique_lock<std::mutex> lock(_mutex);
                return _total_count;
            }
            size_t durable_count()
            {
                std::unique_lock<std::mutex> lock(_mutex);
                return _durable_msgs.size();
            }
            size_t waitack_count()
            {
                std::unique_lock<std::mutex> lock(_mutex);
                return _waitack_msgs.size();
            }
            void clear()
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _mapper.removeMsgFile();
                _msgs.clear();
                _durable_msgs.clear();
                _waitack_msgs.clear();
                _valid_count = 0;
                _total_count = 0;
            }
        private:
            bool GCCheck()
            {
                if (_total_count > 2000 && _valid_count * 10 / _total_count < 5)
                {
                    return true;
                }
                return false;
            }
            void gc()
            {
                if (GCCheck() == false) return;
                std::list<MessagePtr> msgs = _mapper.gc();
                for (auto &msg : msgs)
                {
                    auto it = _durable_msgs.find(msg->payload().properties().id());
                    if (it == _durable_msgs.end())
                    {
                        DLOG("垃圾回收后, 有一条持久化消息, 在内存中没有进行管理!");
                        _msgs.push_back(msg);
                        _durable_msgs.insert(std::make_pair(msg->payload().properties().id(), msg));
                        continue;
                    }
                    it->second->set_offset(msg->offset());
                    it->second->set_length(msg->length());
                }
                _valid_count = _total_count = msgs.size();
            }
            std::mutex _mutex;
            std::string _qname;
            size_t _valid_count;
            size_t _total_count;
            MessageMapper _mapper;
            std::list<MessagePtr> _msgs;                               // 待推送消息
            std::unordered_map<std::string, MessagePtr> _durable_msgs; // 持久化消息hash
            std::unordered_map<std::string, MessagePtr> _waitack_msgs; // 待确认消息hash
    };

    class MessageManager {
        public:
            using ptr = std::shared_ptr<MessageManager>;
            MessageManager(const std::string &basedir):_basedir(basedir){}
            void clear()
            {
                std::unique_lock<std::mutex> lock(_mutex);
                for (auto &qmsg : _queue_msgs)
                {
                    qmsg.second->clear();
                }
            }
            void initQueueMessage(const std::string &qname)
            {
                QueueMessage::ptr qmp;
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _queue_msgs.find(qname);
                    if (it != _queue_msgs.end())
                    {
                        return;
                    }
                    qmp = std::make_shared<QueueMessage>(_basedir, qname);
                    _queue_msgs.insert(std::make_pair(qname, qmp));
                }
                qmp->recovery();
            }
            void destroyQueueMessage(const std::string &qname)
            {
                QueueMessage::ptr qmp;
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _queue_msgs.find(qname);
                    if (it == _queue_msgs.end())
                    {
                        return;
                    }
                    qmp = it->second;
                    _queue_msgs.erase(it);
                }
                qmp->clear();
            }
            bool insert(const std::string &qname, BasicProperties *bp, const std::string &body, bool queue_is_durable)
            {
                QueueMessage::ptr qmp;
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _queue_msgs.find(qname);
                    if (it == _queue_msgs.end())
                    {
                        DLOG("向队列%s新增消息失败：没有找到消息管理句柄!", qname.c_str());
                        return false;
                    }
                    qmp = it->second;
                }
                return qmp->insert(bp, body, queue_is_durable);
            }
            MessagePtr front(const std::string &qname)
            {
                QueueMessage::ptr qmp;
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _queue_msgs.find(qname);
                    if (it == _queue_msgs.end())
                    {
                        DLOG("获取队列%s队首消息失败：没有找到消息管理句柄!", qname.c_str());
                        return MessagePtr();
                    }
                    qmp = it->second;
                }
                return qmp->front();
            }
            void ack(const std::string &qname, const std::string &msg_id)
            {
                QueueMessage::ptr qmp;
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _queue_msgs.find(qname);
                    if (it == _queue_msgs.end())
                    {
                        DLOG("确认队列%s消息%s失败：没有找到消息管理句柄!", qname.c_str(), msg_id.c_str());
                        return;
                    }
                    qmp = it->second;
                }
                qmp->remove(msg_id);
                return;
            }

            size_t getable_count(const std::string &qname)
            {
                QueueMessage::ptr qmp;
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _queue_msgs.find(qname);
                    if (it == _queue_msgs.end())
                    {
                        DLOG("获取队列%s待推送消息数量失败：没有找到消息管理句柄!", qname.c_str());
                        return 0;
                    }
                    qmp = it->second;
                }
                return qmp->getable_count();
            }
            size_t total_count(const std::string &qname)
            {
                QueueMessage::ptr qmp;
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _queue_msgs.find(qname);
                    if (it == _queue_msgs.end())
                    {
                        DLOG("获取队列%s总持久化消息数量失败：没有找到消息管理句柄!", qname.c_str());
                        return 0;
                    }
                    qmp = it->second;
                }
                return qmp->total_count();
            }
            size_t durable_count(const std::string &qname)
            {
                QueueMessage::ptr qmp;
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _queue_msgs.find(qname);
                    if (it == _queue_msgs.end())
                    {
                        DLOG("获取队列%s有效持久化消息数量失败：没有找到消息管理句柄!", qname.c_str());
                        return 0;
                    }
                    qmp = it->second;
                }
                return qmp->durable_count();
            }
            size_t waitack_count(const std::string &qname)
            {
                QueueMessage::ptr qmp;
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _queue_msgs.find(qname);
                    if (it == _queue_msgs.end())
                    {
                        DLOG("获取队列%s待确认消息数量失败：没有找到消息管理句柄!", qname.c_str());
                        return 0;
                    }
                    qmp = it->second;
                }
                return qmp->waitack_count();
            }

        private:
            std::mutex _mutex;
            std::string _basedir;
            std::unordered_map<std::string, QueueMessage::ptr> _queue_msgs;
    };
}


#endif

