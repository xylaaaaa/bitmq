#ifndef BITMQ_MQCOMMON_MQ_HELPER_HPP
#define BITMQ_MQCOMMON_MQ_HELPER_HPP
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <random>
#include <sstream>
#include <iomanip>
#include <atomic>
#include <sqlite3.h>
#include <cstring>
#include <cerrno>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include "mq_logger.hpp"

namespace bitmq
{
    class SqliteHelper
    {
    public:
        typedef int (*SqliteCallback)(void *, int, char **, char **);
        SqliteHelper(const std::string &dbfile) : _dbfile(dbfile), _handler(nullptr) {}

        bool open(int safe_leve = SQLITE_OPEN_FULLMUTEX)
        {
            // int sqlite3_open_v2(const char *filename, sqlite3 **ppDb, int flags, const char *zVfs );
            int ret = sqlite3_open_v2(_dbfile.c_str(), &_handler, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | safe_leve, nullptr);
            if (ret != SQLITE_OK)
            {
                ELOG("创建/打开sqlite数据库失败: %s", sqlite3_errmsg(_handler));
                return false;
            }
            return true;
        }
        bool exec(const std::string &sql, SqliteCallback cb, void *arg)
        {
            // int sqlite3_exec(sqlite3*, char *sql, int (*callback)(void*,int,char**,char**), void* arg, char **err)
            int ret = sqlite3_exec(_handler, sql.c_str(), cb, arg, nullptr);
            if (ret != SQLITE_OK)
            {
                ELOG("%s \n语句执行失败: %s", sql.c_str(), sqlite3_errmsg(_handler));
                return false;
            }
            return true;
        }
        void close()
        {
            if (_handler)
                sqlite3_close_v2(_handler);
        }

    private:
        std::string _dbfile;
        sqlite3 *_handler;
    };

    class StrHelper
    {
    public:
        static size_t split(const std::string &str, const std::string &sep, std::vector<std::string> &result)
        {
            if (sep.empty())
            {
                if (!str.empty())
                {
                    result.push_back(str);
                }
                return result.size();
            }
            size_t pos, idx = 0;
            while (idx < str.size())
            {
                pos = str.find(sep, idx);
                if (pos == std::string::npos)
                {
                    result.push_back(str.substr(idx));
                    return result.size();
                }
                if (pos == idx)
                {
                    idx = pos + sep.size();
                    continue;
                }
                result.push_back(str.substr(idx, pos - idx));
                idx = pos + sep.size();
            }
            return result.size();
        }
    };
    class UUIDHelper
    {
    public:
        static std::string uuid()
        {
            std::random_device rd;
            std::mt19937_64 gernator(rd());
            std::uniform_int_distribution<int> distribution(0, 255);
            std::stringstream ss;
            for (int i = 0; i < 8; i++)
            {
                ss << std::setw(2) << std::setfill('0') << std::hex << distribution(gernator);
                if (i == 3 || i == 5 || i == 7)
                {
                    ss << "-";
                }
            }
            static std::atomic<size_t> seq(1);
            size_t num = seq.fetch_add(1);
            for (int i = 7; i >= 0; i--)
            {
                ss << std::setw(2) << std::setfill('0') << std::hex << ((num >> (i * 8)) & 0xff);
                if (i == 6)
                    ss << "-";
            }
            return ss.str();
        }
    };

    class FileHelper
    {
    public:
        FileHelper(const std::string &filename) : _filename(filename) {}
        bool exists()
        {
            struct stat st;
            return (stat(_filename.c_str(), &st) == 0);
        }
        size_t size()
        {
            struct stat st;
            int ret = stat(_filename.c_str(), &st);
            if (ret < 0)
            {
                return 0;
            }
            return st.st_size;
        }
        bool read(char *body, size_t offset, size_t len)
        {
            // 1. 打开文件
            std::ifstream ifs(_filename, std::ios::binary | std::ios::in);
            if (ifs.is_open() == false)
            {
                ELOG("%s 文件打开失败！", _filename.c_str());
                return false;
            }
            // 2. 跳转文件读写位置
            ifs.seekg(offset, std::ios::beg);
            // 3. 读取文件数据
            ifs.read(body, len);
            if (ifs.good() == false)
            {
                ELOG("%s 文件读取数据失败！！", _filename.c_str());
                ifs.close();
                return false;
            }
            // 4. 关闭文件
            ifs.close();
            return true;
        }
        bool read(std::string &body)
        {
            // 获取文件大小，根据文件大小调整body的空间
            size_t fsize = this->size();
            body.resize(fsize);
            return read(&body[0], 0, fsize);
        }
        bool write(const char *body, size_t offset, size_t len)
        {
            if (body == nullptr && len != 0)
            {
                ELOG("%s 写入参数非法: body 为空且 len 非 0", _filename.c_str());
                return false;
            }
            if (!exists())
            {
                if (!createFile(_filename))
                {
                    return false;
                }
            }
            // 1. 打开文件
            std::fstream fs(_filename, std::ios::binary | std::ios::in | std::ios::out);
            if (fs.is_open() == false)
            {
                ELOG("%s 文件打开失败！", _filename.c_str());
                return false;
            }
            // 2. 跳转到文件指定位置
            fs.seekp(offset, std::ios::beg);
            // 3. 写入数据
            fs.write(body, len);
            if (fs.good() == false)
            {
                ELOG("%s 文件写入数据失败！！", _filename.c_str());
                fs.close();
                return false;
            }
            // 4. 关闭文件
            fs.close();
            return true;
        }
        bool write(const std::string &body)
        {
            return write(body.c_str(), 0, body.size());
        }
        bool rename(const std::string &nname)
        {
            return (::rename(_filename.c_str(), nname.c_str()) == 0);
        }
        static std::string parentDirectory(const std::string &filename)
        {
            // /aaa/bb/ccc/ddd/test.txt
            size_t pos = filename.find_last_of("/");
            if (pos == std::string::npos)
            {
                // test.txt
                return "./";
            }
            std::string path = filename.substr(0, pos);
            return path;
        }
        static bool createFile(const std::string &filename)
        {
            std::fstream ofs(filename, std::ios::binary | std::ios::out);
            if (ofs.is_open() == false)
            {
                ELOG("%s 文件打开失败！", filename.c_str());
                return false;
            }
            ofs.close();
            return true;
        }
        static bool removeFile(const std::string &filename)
        {
            return (::remove(filename.c_str()) == 0);
        }
        static bool createDirectory(const std::string &path)
        {
            if (path.empty())
            {
                ELOG("创建目录失败: 路径为空");
                return false;
            }
            std::string target = path;
            while (target.size() > 1 && target.back() == '/')
            {
                target.pop_back();
            }
            struct stat st;
            if (stat(target.c_str(), &st) == 0)
            {
                return S_ISDIR(st.st_mode);
            }

            std::string current;
            size_t idx = 0;
            if (!target.empty() && target[0] == '/')
            {
                current = "/";
                idx = 1;
            }

            while (idx <= target.size())
            {
                size_t pos = target.find("/", idx);
                std::string part = (pos == std::string::npos) ? target.substr(idx) : target.substr(idx, pos - idx);
                if (!part.empty())
                {
                    if (!current.empty() && current.back() != '/')
                    {
                        current += "/";
                    }
                    current += part;
                    int ret = mkdir(current.c_str(), 0775);
                    if (ret != 0 && errno != EEXIST)
                    {
                        ELOG("创建目录 %s 失败: %s", current.c_str(), strerror(errno));
                        return false;
                    }
                }
                if (pos == std::string::npos)
                {
                    break;
                }
                idx = pos + 1;
            }
            return true;
        }
        static bool removeDirectory(const std::string &path)
        {
            if (path.empty())
            {
                ELOG("删除目录失败: 路径为空");
                return false;
            }

            struct stat st;
            if (lstat(path.c_str(), &st) != 0)
            {
                if (errno == ENOENT)
                {
                    return true;
                }
                ELOG("获取目录 %s 状态失败: %s", path.c_str(), strerror(errno));
                return false;
            }
            if (!S_ISDIR(st.st_mode))
            {
                ELOG("删除目录失败: %s 不是目录", path.c_str());
                return false;
            }

            DIR *dir = opendir(path.c_str());
            if (dir == nullptr)
            {
                ELOG("打开目录 %s 失败: %s", path.c_str(), strerror(errno));
                return false;
            }

            struct dirent *entry = nullptr;
            while ((entry = readdir(dir)) != nullptr)
            {
                const char *name = entry->d_name;
                if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
                {
                    continue;
                }

                std::string child = path;
                if (!child.empty() && child.back() != '/')
                {
                    child += "/";
                }
                child += name;

                struct stat cst;
                if (lstat(child.c_str(), &cst) != 0)
                {
                    ELOG("获取路径 %s 状态失败: %s", child.c_str(), strerror(errno));
                    closedir(dir);
                    return false;
                }

                if (S_ISDIR(cst.st_mode))
                {
                    if (!removeDirectory(child))
                    {
                        closedir(dir);
                        return false;
                    }
                }
                else
                {
                    if (::remove(child.c_str()) != 0)
                    {
                        ELOG("删除文件 %s 失败: %s", child.c_str(), strerror(errno));
                        closedir(dir);
                        return false;
                    }
                }
            }

            closedir(dir);
            if (rmdir(path.c_str()) != 0)
            {
                ELOG("删除目录 %s 失败: %s", path.c_str(), strerror(errno));
                return false;
            }
            return true;
        }

    private:
        std::string _filename;
    };
}

#endif
