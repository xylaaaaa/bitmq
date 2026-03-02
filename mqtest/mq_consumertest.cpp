#include "../mqserver/mq_consumer.hpp"
#include <gtest/gtest.h>

bitmq::ConsumerManager::ptr cmp;

class ConsumerTest : public testing::Environment
{
public:
    void SetUp() override
    {
        cmp = std::make_shared<bitmq::ConsumerManager>();
        cmp->initQueueConsumer("q1");
    }
    void TearDown() override
    {
        cmp->clear();
    }
};

TEST(consumer_test, create_exists_remove_empty_test)
{
    auto cb = [](const std::string &, const bitmq::BasicProperties *, const std::string &) {};
    ASSERT_TRUE(cmp->empty("q1"));

    auto c1 = cmp->create("c1", "q1", true, cb);
    ASSERT_NE(c1.get(), nullptr);
    ASSERT_TRUE(cmp->exists("c1", "q1"));
    ASSERT_FALSE(cmp->empty("q1"));

    cmp->remove("c1", "q1");
    ASSERT_FALSE(cmp->exists("c1", "q1"));
    ASSERT_TRUE(cmp->empty("q1"));
}

TEST(consumer_test, round_robin_choose_test)
{
    auto cb = [](const std::string &, const bitmq::BasicProperties *, const std::string &) {};
    ASSERT_NE(cmp->create("c1", "q1", true, cb).get(), nullptr);
    ASSERT_NE(cmp->create("c2", "q1", true, cb).get(), nullptr);

    auto p1 = cmp->choose("q1");
    auto p2 = cmp->choose("q1");
    auto p3 = cmp->choose("q1");

    ASSERT_NE(p1.get(), nullptr);
    ASSERT_NE(p2.get(), nullptr);
    ASSERT_NE(p3.get(), nullptr);
    ASSERT_EQ(p1->tag, "c1");
    ASSERT_EQ(p2->tag, "c2");
    ASSERT_EQ(p3->tag, "c1");
}

int main(int argc, char *argv[])
{
    testing::InitGoogleTest(&argc, argv);
    testing::AddGlobalTestEnvironment(new ConsumerTest);
    return RUN_ALL_TESTS();
}
