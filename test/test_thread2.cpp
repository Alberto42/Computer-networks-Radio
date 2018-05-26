// condition_variable example
#include <iostream>           // std::cout
#include <thread>             // std::thread
#include <mutex>              // std::mutex, std::unique_lock
#include <condition_variable> // std::condition_variable

std::mutex mtx;
std::condition_variable cv;

void print_id (int id) {
    std::unique_lock<std::mutex> lck(mtx);
    cv.wait(lck);
    // ...
    std::cout << "thread " << id << '\n';
}

void go() {
    std::unique_lock<std::mutex> lck(mtx);
    cv.notify_all();
}

int main ()
{
    std::thread threads[1];
    // spawn 10 threads:
    for (int i=0; i<1; ++i)
        threads[i] = std::thread(print_id,i);

    std::cout << "10 threads ready to race...\n";

    go();                       // go!

    for (auto& th : threads) th.join();

    return 0;
}