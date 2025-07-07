#include "contacts.pb.h"

int main()
{
    contacts::PeopleInfo conn;
    conn.set_sn(10001);
    conn.set_name("xx");
    std::string str = conn.SerializeAsString();
    contacts::PeopleInfo stu;
    bool ret = stu.ParseFromString(str);
    if (ret == false)
    {
        std::cout << "反序列化失败" << std::endl;
        return -1;
    }
    std::cout << stu.sn() << std::endl;
    std::cout << stu.name() << std::endl;

    return 0;
}

