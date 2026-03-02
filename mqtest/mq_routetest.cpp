#include "../mqserver/mq_route.hpp"
#include <gtest/gtest.h>

TEST(route_test, legality_test)
{
    ASSERT_TRUE(bitmq::Router::isLegalRoutingKey("news.music.pop_1"));
    ASSERT_FALSE(bitmq::Router::isLegalRoutingKey("news.music.*"));

    ASSERT_TRUE(bitmq::Router::isLegalBindingkey("news.music.*"));
    ASSERT_TRUE(bitmq::Router::isLegalBindingkey("news.#"));
    ASSERT_FALSE(bitmq::Router::isLegalBindingkey("news.music#.*"));
    ASSERT_FALSE(bitmq::Router::isLegalBindingkey("news.*.#"));
}

TEST(route_test, direct_and_fanout_test)
{
    ASSERT_TRUE(bitmq::Router::route(bitmq::ExchangeType::DIRECT, "a.b", "a.b"));
    ASSERT_FALSE(bitmq::Router::route(bitmq::ExchangeType::DIRECT, "a.b", "a.c"));

    ASSERT_TRUE(bitmq::Router::route(bitmq::ExchangeType::FANOUT, "any.key", "whatever"));
}

TEST(route_test, topic_test)
{
    ASSERT_TRUE(bitmq::Router::route(bitmq::ExchangeType::TOPIC, "news.music.pop", "news.music.*"));
    ASSERT_TRUE(bitmq::Router::route(bitmq::ExchangeType::TOPIC, "news.music.pop.rock", "news.music.#"));
    ASSERT_TRUE(bitmq::Router::route(bitmq::ExchangeType::TOPIC, "news", "news.#"));
    ASSERT_TRUE(bitmq::Router::route(bitmq::ExchangeType::TOPIC, "news.sport.football", "news.#"));

    ASSERT_FALSE(bitmq::Router::route(bitmq::ExchangeType::TOPIC, "news.music.pop", "news.sport.#"));
    ASSERT_FALSE(bitmq::Router::route(bitmq::ExchangeType::TOPIC, "a.b", "a.*.*"));
}

int main(int argc, char *argv[])
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
