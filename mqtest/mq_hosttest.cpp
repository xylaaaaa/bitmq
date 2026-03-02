#include "../mqserver/mq_host.hpp"
#include <gtest/gtest.h>

class HostTest : public testing::Test
{
protected:
    void SetUp() override
    {
        bitmq::FileHelper::removeDirectory(base_dir_);
        bitmq::FileHelper::removeFile(db_file_);
        host_ = std::make_shared<bitmq::VirtualHost>("test_host", base_dir_, db_file_);
    }

    void TearDown() override
    {
        host_->clear();
        host_.reset();
        bitmq::FileHelper::removeDirectory(base_dir_);
        bitmq::FileHelper::removeFile(db_file_);
    }

    bitmq::VirtualHost::ptr host_;
    std::string base_dir_ = "./data/host_message/";
    std::string db_file_ = "./data/host_meta.db";
};

TEST_F(HostTest, declare_bind_publish_consume_ack_test)
{
    google::protobuf::Map<std::string, std::string> args;
    ASSERT_TRUE(host_->declareExchange("ex1", bitmq::ExchangeType::DIRECT, true, false, args));
    ASSERT_TRUE(host_->declareQueue("q1", true, false, false, args));
    ASSERT_TRUE(host_->bind("ex1", "q1", "rk1"));

    bitmq::BasicProperties bp;
    bp.set_id("msg-1");
    bp.set_delivery_mode(bitmq::DeliveryMode::DURABLE);
    bp.set_routing_key("rk1");

    ASSERT_TRUE(host_->basicPublish("q1", &bp, "hello bitmq"));
    auto msg = host_->basicConsume("q1");
    ASSERT_NE(msg.get(), nullptr);
    ASSERT_EQ(msg->payload().body(), "hello bitmq");
    ASSERT_EQ(msg->payload().properties().id(), "msg-1");

    host_->basicAck("q1", "msg-1");
    ASSERT_EQ(host_->basicConsume("q1").get(), nullptr);
}

TEST_F(HostTest, delete_exchange_should_clear_bindings)
{
    google::protobuf::Map<std::string, std::string> args;
    ASSERT_TRUE(host_->declareExchange("ex1", bitmq::ExchangeType::DIRECT, true, false, args));
    ASSERT_TRUE(host_->declareQueue("q1", true, false, false, args));
    ASSERT_TRUE(host_->bind("ex1", "q1", "rk1"));
    ASSERT_TRUE(host_->existsBinding("ex1", "q1"));

    host_->deleteExchange("ex1");
    ASSERT_FALSE(host_->existsBinding("ex1", "q1"));
}

TEST_F(HostTest, delete_queue_should_clear_related_bindings)
{
    google::protobuf::Map<std::string, std::string> args;
    ASSERT_TRUE(host_->declareExchange("ex1", bitmq::ExchangeType::DIRECT, true, false, args));
    ASSERT_TRUE(host_->declareQueue("q1", true, false, false, args));
    ASSERT_TRUE(host_->declareQueue("q2", true, false, false, args));
    ASSERT_TRUE(host_->bind("ex1", "q1", "rk1"));
    ASSERT_TRUE(host_->bind("ex1", "q2", "rk2"));

    host_->deleteQueue("q1");
    ASSERT_FALSE(host_->existsBinding("ex1", "q1"));
    ASSERT_TRUE(host_->existsBinding("ex1", "q2"));
}

int main(int argc, char *argv[])
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
