#ifndef __M_ROUTE_H__
#define __M_ROUTE_H__
#include <iostream>
#include "../mqcommon/mq_logger.hpp"
#include "../mqcommon/mq_helper.hpp"
#include "../mqcommon/mq_msg.pb.h"

namespace bitmq
{
    class Router
    {
    public:
        static bool isLegalRoutingKey(const std::string &routing_key)
        {
            for (auto &ch : routing_key)
            {
                if ((ch >= 'a' && ch <= 'z') ||
                    (ch >= 'A' && ch <= 'Z') ||
                    (ch >= '0' && ch <= '9') ||
                    (ch == '_' || ch == '.'))
                {
                    continue;
                }
                return false;
            }
            return true;
        }
        static bool isLegalBindingkey(const std::string &binding_key)
        {
            for (auto &ch : binding_key)
            {
                if ((ch >= 'a' && ch <= 'z') ||
                    (ch >= 'A' && ch <= 'Z') ||
                    (ch >= '0' && ch <= '9') ||
                    (ch == '_' || ch == '.') ||
                    (ch == '*' || ch == '#'))
                {
                    continue;
                }
                return false;
            }
            // 2. *和#必须独立存在:  news.music#.*.#
            std::vector<std::string> sub_words;
            StrHelper::split(binding_key, ".", sub_words);
            for (auto &word : sub_words)
            {
                if (word.size() > 1 &&
                    (word.find("*") != std::string::npos ||
                     word.find("#") != std::string::npos))
                {
                    return false;
                }
            }
            for (int i = 1; i < sub_words.size(); i++)
            {
                if (sub_words[i] == "#" && sub_words[i - 1] == "*")
                {
                    return false;
                }
                if (sub_words[i] == "#" && sub_words[i - 1] == "#")
                {
                    return false;
                }
                if (sub_words[i] == "*" && sub_words[i - 1] == "#")
                {
                    return false;
                }
            }
            return true;
        }
        static bool route(ExchangeType type, const std::string &routing_key, const std::string &binding_key)
        {
            if (type == ExchangeType::DIRECT)
            {
                return (routing_key == binding_key);
            }
            else if (type == ExchangeType::FANOUT)
            {
                return true;
            }
            std::vector<std::string> bkeys, rkeys;
            int n_bkey = StrHelper::split(binding_key, ".", bkeys);
            int n_rkey = StrHelper::split(routing_key, ".", rkeys);
            std::vector<std::vector<bool>> dp(n_bkey + 1, std::vector<bool>(n_rkey + 1, false));
            dp[0][0] = true;
            for (int i = 1; i <= n_bkey; i++) // 如果binding_key以 #起始，则将 #对应行的第0列置为1.
            {
                if (bkeys[i - 1] == "#")
                {
                    dp[i][0] = dp[i - 1][0];
                    continue;
                }
                break;
            }
            for (int i = 1; i <= n_bkey; i++)
            {
                for (int j = 1; j <= n_rkey; j++)
                {
                    if (bkeys[i - 1] == rkeys[j - 1] || bkeys[i - 1] == "*")
                    {
                        dp[i][j] = dp[i - 1][j - 1];
                    }
                    else if (bkeys[i - 1] == "#")
                    {
                        dp[i][j] = dp[i - 1][j] || dp[i][j - 1];
                    }
                }
            }
            return dp[n_bkey][n_rkey];
        }
    };
}
#endif
