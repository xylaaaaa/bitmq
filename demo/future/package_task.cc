#include <iostream>
#include <thread>
#include <future>
#include <memory>

int Add(int a, int b)
{
    std::this_thread::sleep_for(std::chrono::seconds(3));
    return a + b;
}

int main()
{
    // std::packaged_task<int(int,int)> task(Add);
    // std::future<int> fu = task.get_future();

    // task(11, 22);  task可以当作一个可调用对象来调用执行任务
    // 但是它又不能完全的当作一个函数来使用
    // std::async(std::launch::async, task, 11, 22);
    // std::thread thr(task, 11, 22);

    // 但是我们可以把task定义成为一个指针，传递到线程中，然后进行解引用执行
    // 但是如果单纯指针指向一个对象，存在生命周期的问题，很有可能出现风险
    // 思想就是在堆上new对象，用智能指针管理它的生命周期
    auto ptask = std::make_shared<std::packaged_task<int(int, int)>>(Add);
    std::future<int> res = ptask->get_future();
    std::thread t([ptask](){(*ptask)(11, 22);});

    int sum = res.get();
    std::cout << "11 + 22 = " << sum << std::endl;
    t.join();
    return 0;
}
