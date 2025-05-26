#include <iostream>
#include <future>
#include <thread>

void Add(int num1, int num2, std::promise<int> &prom)
{
    std::this_thread::sleep_for(std::chrono::seconds(3));
    prom.set_value(num1 + num2);
    return;
}

int main()
{
    std::promise<int> prom;
    std::future<int> fu = prom.get_future();
    std::thread t(Add, 11, 22, std::ref(prom));
    int res = fu.get();
    std::cout << "11 + 22 = " << res << std::endl;
    t.join();
    return 0;
}