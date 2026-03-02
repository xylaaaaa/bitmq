#include "../mqserver/mq_exchange.hpp"
#include <gtest/gtest.h>

bitmq::ExchangeManager::ptr emp;
static const std::string kExchangeDb = "./data/exchange_meta.db";

class ExchangeTest : public testing::Environment
{
public:
    virtual void SetUp() override
    {
        bitmq::FileHelper::removeFile(kExchangeDb);
        emp = std::make_shared<bitmq::ExchangeManager>(kExchangeDb);
    }
    virtual void TearDown() override
    {
        emp->clear();
        bitmq::FileHelper::removeFile(kExchangeDb);
    }
};

TEST(exchange_test, declare_select_delete_test)
{
    google::protobuf::Map<std::string, std::string> args;
    args["owner"] = "bitmq";
    ASSERT_TRUE(emp->declareExchange("exchange1", bitmq::ExchangeType::DIRECT, true, false, args));
    ASSERT_TRUE(emp->exists("exchange1"));
    ASSERT_EQ(emp->size(), 1);

    auto ex = emp->selectExchange("exchange1");
    ASSERT_NE(ex.get(), nullptr);
    ASSERT_EQ(ex->name, "exchange1");
    ASSERT_EQ(ex->type, bitmq::ExchangeType::DIRECT);
    ASSERT_TRUE(ex->durable);
    ASSERT_FALSE(ex->auto_delete);
    ASSERT_EQ(ex->args["owner"], "bitmq");

    emp->deleteExchange("exchange1");
    ASSERT_FALSE(emp->exists("exchange1"));
    ASSERT_EQ(emp->size(), 0);
}

TEST(exchange_test, durable_recovery_test)
{
    google::protobuf::Map<std::string, std::string> args;
    ASSERT_TRUE(emp->declareExchange("durable_ex", bitmq::ExchangeType::FANOUT, true, false, args));
    ASSERT_TRUE(emp->declareExchange("transient_ex", bitmq::ExchangeType::DIRECT, false, false, args));
    ASSERT_TRUE(emp->exists("durable_ex"));
    ASSERT_TRUE(emp->exists("transient_ex"));

    emp.reset();
    emp = std::make_shared<bitmq::ExchangeManager>(kExchangeDb);

    ASSERT_TRUE(emp->exists("durable_ex"));
    ASSERT_FALSE(emp->exists("transient_ex"));
}

int main(int argc, char *argv[])
{
    testing::InitGoogleTest(&argc, argv);
    testing::AddGlobalTestEnvironment(new ExchangeTest);
    return RUN_ALL_TESTS();
}
