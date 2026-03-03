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
#include <cassert>
#include <cctype>
#include <cerrno>
#include <cstdlib>

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
            args.clear();
            std::vector<std::string> sub_args;
            StrHelper::split(str_args, "&",  sub_args);
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
            static const char* kHex = "0123456789ABCDEF";
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
                #define DROP_TABLE "drop table if exists exchange_table;"
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
                ss << "'" << escapeSqlLiteral(exp->name) << "', ";
                ss << exp->type << ", ";
                ss << exp->durable << ", ";
                ss << exp->auto_delete << ", ";
                ss << "'" << escapeSqlLiteral(exp->getArgs()) << "');";
                return _sql_helper.exec(ss.str(), nullptr, nullptr);
            }

            void remove(const std::string &name)
            {
                std::stringstream ss;
                ss << "delete from exchange_table where name=";
                ss << "'" << escapeSqlLiteral(name) << "';";
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
                (void)fields;
                ExchangeMap *result = (ExchangeMap*)arg;
                if (result == nullptr || row == nullptr || numcol < 5)
                {
                    return 0;
                }
                if (row[0] == nullptr || row[1] == nullptr || row[2] == nullptr || row[3] == nullptr)
                {
                    ELOG("exchange_table 记录字段缺失，跳过一条记录");
                    return 0;
                }
                int type = 0;
                int durable = 0;
                int auto_delete = 0;
                if (!parseInt(row[1], type) || !parseInt(row[2], durable) || !parseInt(row[3], auto_delete))
                {
                    ELOG("exchange_table 记录字段格式错误，跳过一条记录");
                    return 0;
                }
                if (type < static_cast<int>(UNKNOWTYPE) || type > static_cast<int>(TOPIC))
                {
                    ELOG("exchange_table 记录交换机类型非法，跳过一条记录");
                    return 0;
                }

                auto exp = std::make_shared<Exchange>();
                exp->name = row[0];
                exp->type = static_cast<bitmq::ExchangeType>(type);
                exp->durable = (durable != 0);
                exp->auto_delete = (auto_delete != 0);
                if (row[4])
                    exp->setArgs(row[4]);
                result->insert(std::make_pair(exp->name, exp));
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
            static bool parseInt(const char* text, int &val)
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
                _exchanges.insert(std::make_pair(name, exp));
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
