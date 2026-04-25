#pragma once

#include<mutex>
#include<vector>
#include<thread>
#include<queue>
#include<functional>
#include<condition_variable>
#include <atomic>

class ThreadPool
{
    std::vector<std::thread> threads; //to hold all worker threads
    std::queue<std::function<void()>> jobs; //queue to hold all pending jobs
    std::mutex key; //to avoid race condition, when 2 threads are simultaneously reaching for the same job
    std::condition_variable bell; //to prevent "busy waiting", or to stop the thread from reaching the queue again and again when it is empty.
    std::condition_variable main_bell; //for wait_finished() function
    bool stop = false; //stop condition
    std::atomic<int> jobs_left{0}; //for the wait function
    public:
    ThreadPool(int num_threads)
    {
        for(int i=0;i<num_threads;i++)
        {
            this->threads.emplace_back([this](){
                while(true)
                {
                    std::unique_lock<std::mutex> lock(this->key); //locking the queue
                    this->bell.wait(lock, [this]() {
                        return !this->jobs.empty() || stop;
                    }); //wait function to check for the queue and stop condition and determine whether to make the thread sleep or stop waiting.
                    if(this->jobs.empty() && stop)
                    {return;}
                    std::function<void()> job = jobs.front();
                    jobs.pop();
                    lock.unlock();
                    job();
                    this->jobs_left--;
                    if(this->jobs_left == 0)
                        this->main_bell.notify_one();
                }
            });
        }
    }
    void wait_finished() {
        std::unique_lock<std::mutex> lock(this->key); //locking the queue
        this->main_bell.wait(lock,[this](){
            return this->jobs_left == 0;
        });
    }
    void addjob(std::function<void()> job)
    {
        std::unique_lock<std::mutex> lock(this->key);
        jobs.push(job);
        this->jobs_left++;
        this->bell.notify_one();
    }
    ~ThreadPool()
    {
        std::unique_lock<std::mutex> lock(this->key);
        this->stop = true;
        this->bell.notify_all();
        lock.unlock();
        for(int i =0;i<threads.size();i++)
        {
            threads[i].join();
        }
    }
};