#include <iostream>
#include <thread>
#include <future>
#include <chrono>

int Add(int a, int b)
{
    std::cout << "Add 函数开始执行" << std::endl;
    //std::this_thread::sleep_for(std::chrono::seconds(5));
    std::cout << "Add 函数执行结束" << std::endl;
    return a + b;
}

int main()
{
    std::cout << "-------1-------" << std::endl;
    std::future<int> result = std::async(std::launch::async, Add, 11, 22);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "-------2-------" << std::endl;
    int sum = result.get(); // 阻塞等待，直到结果返回
    std::cout << "-------3-------" << std::endl;
    std::cout << "11 + 22 = " << sum << std::endl;
   
    return 0;
}