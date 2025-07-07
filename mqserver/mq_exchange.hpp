#ifndef __M_EXCHANGE_H__
#define __M_EXCHANGE_H__
#include "../mqcommon/mq_logger.hpp"
#include "../mqcommon/mq_helper.hpp"
#include "../mqcommon/mq_msg.pb.h"
#include <google/protobuf/map.h>
#include <iostream>
#include <unordered_map>
#include <mutex>
#include <memory>

namespace bitmq {
    struct Exchange {
        using ptr = std::shared_ptr<Exchange>;
        std::string name;
        ExchangeType type;
        bool durable;
        bool auto_delete;
        google::protobuf::Map<std::string, std::string> args;

        Exchange() {}
        Exchange(const std::string &ename,
            ExchangeType etype,
            bool edurable,
            bool eauto_delet,
            const google::protobuf::Map<std::string, std::string> &eargs):
            name(ename), type(etype), durable(edurable), 
            auto_delete(eauto_delet), args(eargs){}

        void setArgs(const std::string &str_args)
        {
            std::vector<std::string> sub_args;;
            StrHelper::split(str_args, "&",  sub_args);
            for (auto &str : sub_args)
            {
                size_t pos = str.find("=");
                std::string key = str.substr(0, pos);
                std::string val = str.substr(pos + 1);
                args[key] = val;
            }
        }

        std::string getArgs()
        {
            std::string result;
            for (auto start = args.begin(); start != args.end(); ++start)
            {
                result += start->first + "=" + start->second + "&";
            }
            return result;
        }
    };

    using ExchangeMap = std::unordered_map<std::string, Exchange::ptr>;
    class ExchangeMapper {
        public:
            ExchangeMapper(const std::string &dbfile) : _sql_helper(dbfile) {
                std::string path = FileHelper::parentDirectory(dbfile);
                FileHelper::createDirectory(path);
                assert(_sql_helper.open());
                createTable();
            }
            void createTable()
            {
                #define CREATE_TABLE "create table if not exists exchange_table(\
                    name varchar(32) primary key,\
                    type int, \
                    durable int, \
                    auto_delete int, \
                    args varchar(128));"
                bool ret = _sql_helper.exec(CREATE_TABLE, nullptr, nullptr);
                if (ret == false)
                {
                    DLOG("创建交换机数据库表失败!!\n");
                    abort();
                }
            }
            void removeTable() {
                #define DROP_TABLE "drop table if not exists exchage_table;"
                bool ret = _sql_helper.exec(DROP_TABLE, nullptr, nullptr);
                if (ret == false)
                {
                    DLOG("删除交换机数据库表失败!!\n");
                    abort();
                }
            }
            bool insert(Exchange::ptr &exp)
            {
                std::stringstream ss;
                ss << "insert into exchange_table values(";
                ss << "'" << exp->name << "', ";
                ss << exp->type << ", ";
                ss << exp->durable << ", ";
                ss << exp->auto_delete << ", ";
                ss << "'" << exp->getArgs() << ");";
                return _sql_helper.exec(ss.str(), nullptr, nullptr);
            }

            void remove(const std::string &name)
            {
                std::stringstream ss;
                ss << "delete from exchange_table where name=";
                ss << "'" << name << "';";
                _sql_helper.exec(ss.str(), nullptr, nullptr);
            }

            ExchangeMap recovery() 
            {
                ExchangeMap result;
                std::string sql = "select name, type, durable, auto_delete, args from exchange_table;";
                _sql_helper.exec(sql, selectCallback, &result);
                return result;
            }

        private:
            static int selectCallback(void* arg, int numcol, char** row, char** fields)
            {
                ExchangeMap *result = (ExchangeMap*)arg;
                auto exp = std::make_shared<Exchange>();
                exp->name = row[0];
                exp->type = (bitmq::ExchangeType)std::stoi(row[1]);
                exp->durable = (bool)std::stoi(row[2]);
                exp->auto_delete = (bool)std::stoi(row[3]);
                if (row[4])
                    exp->setArgs(row[4]);
                result->insert(std::make_pair(exp->name, exp));
                return 0;
            }
        SqliteHelper _sql_helper;
    };
    class ExchangeManager {
        public:
            using ptr = std::shared_ptr<ExchangeManager>;
            ExchangeManager(const std::string &dbfile) : _mapper(dbfile){
                _exchanges = _mapper.recovery();
            }

            bool declareExchange(const std::string &name,
                ExchangeType type, bool durable, bool auto_delete,
                const google::protobuf::Map<std::string, std::string> &args)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _exchanges.find(name);
                if (it != _exchanges.end())
                {
                    return true;
                }
                auto exp = std::make_shared<Exchange>(name, type, durable, auto_delete, args);
                if (durable == true)
                {
                    bool ret = _mapper.insert(exp);
                    if (ret == false)
                    {
                        return false;
                    }
                }
                return true;
            }

            void deleteExchange(const std::string &name)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _exchanges.find(name);
                if (it != _exchanges.end())
                {
                    if (it->second->durable == true)
                        _mapper.remove(name);
                    _exchanges.erase(name);
                }
            }

            Exchange::ptr selectExchange(const std::string &name)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _exchanges.find(name);
                if (it == _exchanges.end())
                    return Exchange::ptr();
                return it->second;
            }

            bool exists(const std::string &name)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _exchanges.find(name);
                if (it == _exchanges.end())
                {
                    return false;
                }
                return true;
            }
            size_t size()
            {
                std::unique_lock<std::mutex> lock(_mutex);
                return _exchanges.size();
            }
            void clear()
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _mapper.removeTable();
                _exchanges.clear();
            }

        private:
            std::mutex _mutex;
            ExchangeMapper _mapper;
            ExchangeMap _exchanges;
    };
}


#endif