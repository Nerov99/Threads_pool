#include <QCoreApplication>
#include <iostream>
#include <vector>
#include <queue>
#include <mutex>
#include <future>

class SimpleThreadPool {
public:
    explicit SimpleThreadPool(std::size_t threadCount);
    ~SimpleThreadPool();

    SimpleThreadPool(const SimpleThreadPool&) = delete;
    SimpleThreadPool& operator = (const SimpleThreadPool&) = delete;

    template <typename Fnc_T>
    auto Post(Fnc_T&& task) -> std::future<decltype(task())>;

    void WorkOn();
    void Destroy();

private:
    void Run();
    bool Wait_all();
    size_t m_threadCount;
    std::vector <std::thread> threads;
    std::queue <std::packaged_task <void()>> tasks;
    std::condition_variable condition;
    std::mutex mut;
    bool stop;
};

SimpleThreadPool::SimpleThreadPool(std::size_t threadCount) : m_threadCount{ threadCount }, stop{ true } {
    threads.reserve(m_threadCount);
    for (size_t i = 0; i < m_threadCount; ++i) {
        threads.emplace_back(&SimpleThreadPool::Run, this);
    }
}

SimpleThreadPool::~SimpleThreadPool() {
    if (stop == false) {
        std::unique_lock<std::mutex> lock(mut);
        while (!Wait_all());
        stop = true;
    }
    for (size_t i = 0; i < m_threadCount; ++i) {
        condition.notify_all();
        threads[i].join();
    }
}

template <typename Fnc_T>
auto SimpleThreadPool::Post(Fnc_T&& task) -> std::future<decltype(task())> {
    using R = std::result_of_t < Fnc_T& ()>;

    std::packaged_task<R()> p(std::forward<Fnc_T>(task));
    auto r = p.get_future();

    {
        std::lock_guard<std::mutex> lock(mut);
        tasks.emplace(std::move(p));
    }

    condition.notify_one();
    return r;
}

void SimpleThreadPool::WorkOn() {
    stop = false;
}

void SimpleThreadPool::Destroy() {
    stop = true;
    std::unique_lock<std::mutex> lock(mut);
    while (!tasks.empty())
        tasks.pop();
}

void SimpleThreadPool::Run() {
    while (!stop) {
        std::unique_lock<std::mutex> lock(mut);
        condition.wait(lock, [this]()->bool { return !tasks.empty() || stop; });

        if (!tasks.empty()) {
            auto elem = std::move(tasks.front());
            tasks.pop();
            lock.unlock();

            elem();
        }
    }
}

bool SimpleThreadPool::Wait_all() {
    bool pool_is_busy;
    pool_is_busy = tasks.empty();
    return pool_is_busy;
}

int CheckFuture() {
    std::cout << "Using std::future!\n";
    return 4 * 4;
}

void Calc() {
    for (size_t i = 0; i < 2000000000; ++i);
    std::cout << "Big calculation was completed" << std::endl;
}

class Test {
public:
    void operator() () {
        std::cout << "Working with functors!" << std::endl;
    }
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    SimpleThreadPool Pool(3);
    Test test;

    Pool.Post(Calc);
    auto a = Pool.Post(CheckFuture);
    Pool.Post([]() { std::cout << "Work with lambda object" << std::endl; });
    Pool.Post(test);
    Pool.WorkOn();

    try {
        if (a.valid())
            std::cout << a.get() << std::endl;
        }
    catch(const std::future_error& ex) {
        std::cout << "Function has not yet executed: " << ex.what() << std::endl;
    }

    return app.exec();
}
