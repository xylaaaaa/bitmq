#ifndef BITMQ_MQSERVER_MQ_QUEUE_HPP
#define BITMQ_MQSERVER_MQ_QUEUE_HPP
#include "../mqcommon/mq_logger.hpp"
#include "../mqcommon/mq_helper.hpp"
#include "../mqcommon/mq_msg.pb.h"
#include <iostream>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <cassert>
#include <cctype>
#include <cerrno>
#include <cstdlib>

namespace bitmq
{
    struct MsgQueue
    {
        using ptr = std::shared_ptr<MsgQueue>;
        std::string name;
        bool durable;
        bool exclusive;
        bool auto_delete;
        google::protobuf::Map<std::string, std::string> args;

        MsgQueue() {}
        MsgQueue(const std::string &qname,
                 bool qdurable,
                 bool qexclusive,
                 bool qauto_delete,
                 const google::protobuf::Map<std::string, std::string> &qargs) : name(qname), durable(qdurable), exclusive(qexclusive),
                                                                                 auto_delete(qauto_delete), args(qargs) {}
        void setArgs(const std::string &str_args)
        {
            args.clear();
            std::vector<std::string> sub_args;
            StrHelper::split(str_args, "&", sub_args);
            for (auto &str : sub_args)
            {
                size_t pos = str.find("=");
                if (pos == std::string::npos)
                {
                    continue;
                }
                std::string key = str.substr(0, pos);
                std::string val = str.substr(pos + 1);
                key = decodeArgToken(key);
                val = decodeArgToken(val);
                if (key.empty())
                {
                    continue;
                }
                args[key] = val;
            }
        }
        std::string getArgs()
        {
            std::string result;
            bool first = true;
            for (auto start = args.begin(); start != args.end(); ++start)
            {
                if (!first)
                {
                    result += "&";
                }
                first = false;
                result += encodeArgToken(start->first);
                result += "=";
                result += encodeArgToken(start->second);
            }
            return result;
        }

    private:
        static std::string encodeArgToken(const std::string &input)
        {
            static const char *kHex = "0123456789ABCDEF";
            std::string out;
            out.reserve(input.size() * 3);
            for (unsigned char ch : input)
            {
                if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~')
                {
                    out.push_back(static_cast<char>(ch));
                }
                else
                {
                    out.push_back('%');
                    out.push_back(kHex[(ch >> 4) & 0x0f]);
                    out.push_back(kHex[ch & 0x0f]);
                }
            }
            return out;
        }
        static int hexToInt(char c)
        {
            if (c >= '0' && c <= '9')
                return c - '0';
            if (c >= 'a' && c <= 'f')
                return c - 'a' + 10;
            if (c >= 'A' && c <= 'F')
                return c - 'A' + 10;
            return -1;
        }
        static std::string decodeArgToken(const std::string &input)
        {
            std::string out;
            out.reserve(input.size());
            for (size_t i = 0; i < input.size(); i++)
            {
                char ch = input[i];
                if (ch == '%' && i + 2 < input.size())
                {
                    int hi = hexToInt(input[i + 1]);
                    int lo = hexToInt(input[i + 2]);
                    if (hi >= 0 && lo >= 0)
                    {
                        out.push_back(static_cast<char>((hi << 4) | lo));
                        i += 2;
                        continue;
                    }
                }
                out.push_back(ch);
            }
            return out;
        }
    };

    using QueueMap = std::unordered_map<std::string, MsgQueue::ptr>;
    class MsgQueueMapper
    {
    public:
        MsgQueueMapper(const std::string &dbfile) : _sql_helper(dbfile)
        {
            std::string path = FileHelper::parentDirectory(dbfile);
            FileHelper::createDirectory(path);
            assert(_sql_helper.open());
            createTable();
        }
        void createTable()
        {
            std::stringstream sql;
            sql << "create table if not exists queue_table(";
            sql << "name varchar(32) primary key, ";
            sql << "durable int, ";
            sql << "exclusive int, ";
            sql << "auto_delete int, ";
            sql << "args varchar(128));";
            assert(_sql_helper.exec(sql.str(), nullptr, nullptr));
        }
        void removeTable()
        {
            std::string sql = "drop table if exists queue_table;";
            _sql_helper.exec(sql, nullptr, nullptr);
        }
        bool insert(MsgQueue::ptr &queue)
        {
            // insert into queue_table values('queue1', true, false, false, "k1=v1&k2=v2&");
            std::stringstream sql;
            sql << "insert into queue_table values(";
            sql << "'" << escapeSqlLiteral(queue->name) << "', ";
            sql << queue->durable << ", ";
            sql << queue->exclusive << ", ";
            sql << queue->auto_delete << ", ";
            sql << "'" << escapeSqlLiteral(queue->getArgs()) << "');";
            return _sql_helper.exec(sql.str(), nullptr, nullptr);
        }
        void remove(const std::string &name)
        {
            // delete from queue_table where name='queue1';
            std::stringstream sql;
            sql << "delete from queue_table where name=";
            sql << "'" << escapeSqlLiteral(name) << "';";
            _sql_helper.exec(sql.str(), nullptr, nullptr);
        }
        QueueMap recovery()
        {
            QueueMap result;
            std::string sql = "select name, durable, exclusive, auto_delete, args from queue_table;";
            _sql_helper.exec(sql, selectCallback, &result);
            return result;
        }

    private:
        static int selectCallback(void *arg, int numcol, char **row, char **fields)
        {
            (void)fields;
            QueueMap *result = (QueueMap *)arg;
            if (result == nullptr || row == nullptr || numcol < 5)
            {
                return 0;
            }
            if (row[0] == nullptr || row[1] == nullptr || row[2] == nullptr || row[3] == nullptr)
            {
                ELOG("queue_table 记录字段缺失，跳过一条记录");
                return 0;
            }
            int durable = 0;
            int exclusive = 0;
            int auto_delete = 0;
            if (!parseInt(row[1], durable) || !parseInt(row[2], exclusive) || !parseInt(row[3], auto_delete))
            {
                ELOG("queue_table 记录字段格式错误，跳过一条记录");
                return 0;
            }
            MsgQueue::ptr mqp = std::make_shared<MsgQueue>();
            mqp->name = row[0];
            mqp->durable = (durable != 0);
            mqp->exclusive = (exclusive != 0);
            mqp->auto_delete = (auto_delete != 0);
            if (row[4])
                mqp->setArgs(row[4]);
            result->insert(std::make_pair(mqp->name, mqp));
            return 0;
        }
        static std::string escapeSqlLiteral(const std::string &input)
        {
            std::string escaped;
            escaped.reserve(input.size() + 8);
            for (char ch : input)
            {
                if (ch == '\'')
                {
                    escaped += "''";
                }
                else
                {
                    escaped.push_back(ch);
                }
            }
            return escaped;
        }
        static bool parseInt(const char *text, int &val)
        {
            if (text == nullptr)
            {
                return false;
            }
            errno = 0;
            char *end = nullptr;
            long v = std::strtol(text, &end, 10);
            if (errno != 0 || end == text || *end != '\0')
            {
                return false;
            }
            val = static_cast<int>(v);
            return true;
        }

    private:
        SqliteHelper _sql_helper;
    };

    class MsgQueueManager
    {
    public:
        using ptr = std::shared_ptr<MsgQueueManager>;
        MsgQueueManager(const std::string &dbfile) : _mapper(dbfile)
        {
            _msg_queues = _mapper.recovery();
        }
        bool declareQueue(const std::string &qname,
                          bool qdurable,
                          bool qexclusive,
                          bool qauto_delete,
                          const google::protobuf::Map<std::string, std::string> &qargs)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _msg_queues.find(qname);
            if (it != _msg_queues.end())
            {
                return true;
            }
            MsgQueue::ptr mqp = std::make_shared<MsgQueue>();
            mqp->name = qname;
            mqp->durable = qdurable;
            mqp->exclusive = qexclusive;
            mqp->auto_delete = qauto_delete;
            mqp->args = qargs;
            if (qdurable == true)
            {
                bool ret = _mapper.insert(mqp);
                if (ret == false)
                    return false;
            }
            _msg_queues.insert(std::make_pair(qname, mqp));
            return true;
        }
        void deleteQueue(const std::string &name)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _msg_queues.find(name);
            if (it == _msg_queues.end())
            {
                return;
            }
            if (it->second->durable == true)
                _mapper.remove(name);
            _msg_queues.erase(name);
        }
        MsgQueue::ptr selectQueue(const std::string &name)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _msg_queues.find(name);
            if (it == _msg_queues.end())
            {
                return MsgQueue::ptr();
            }
            return it->second;
        }
        QueueMap allQueues()
        {
            std::unique_lock<std::mutex> lock(_mutex);
            return _msg_queues;
        }
        bool exists(const std::string &name)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _msg_queues.find(name);
            if (it == _msg_queues.end())
            {
                return false;
            }
            return true;
        }
        size_t size()
        {
            std::unique_lock<std::mutex> lock(_mutex);
            return _msg_queues.size();
        }
        void clear()
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _mapper.removeTable();
            _msg_queues.clear();
        }

    private:
        std::mutex _mutex;
        MsgQueueMapper _mapper;
        QueueMap _msg_queues;
    };
}
#endif
