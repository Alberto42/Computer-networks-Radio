#include <iostream>
#include <thread>
#include <condition_variable>
#include <zconf.h>

using namespace std;
std::mutex mtx;
condition_variable cv;
void function() {
    std::unique_lock<std::mutex> lck(mtx);
    cout<<"Przed czekaniem" << endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    cv.wait(lck);
    cout<<"Po czekaniu" << endl;
}
int main() {
    thread t{function};
    cout<<"Hello"<<endl;
    std::unique_lock<std::mutex> lck(mtx);
    cout<<"Przed notyfikacja" << endl;
    cv.notify_all();
    lck.unlock();
    cout<<"Po notyfikacja" << endl;
    t.join();
}
