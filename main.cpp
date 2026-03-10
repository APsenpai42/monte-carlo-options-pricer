#include<iostream>
#include "options.h"
#include<memory>
#include<thread>
#include<stdexcept>

int main() {
        unsigned int num_threads = std::thread::hardware_concurrency();
        num_threads = num_threads == 0? 4 : num_threads;
        //num_threads = num_threads>max_threads? max_threads : num_threads;
        try{
            std::unique_ptr<Option> EuOption = std::make_unique<EuropeanOption>(100,100,0.05,0.2,1,"Call");
            std::unique_ptr<Option> AOption = std::make_unique<AsianOption>(100,100,0.05,0.2,1,"Call");
            double euprice = 0, aprice = 0;  
            std::thread t1([&EuOption, &euprice, num_threads]() {
                euprice =  EuOption->payoff(1000000, num_threads/2);
            });
            std::thread t2([&AOption, &aprice, num_threads]() {
                aprice =  AOption->payoff(1000000, num_threads/2);
            });
            t1.join();
            t2.join();
            std::cout<<euprice<<"\n";
            std::cout<<aprice<<"\n";
            // std::cout<<E.payoff(1000000,num_threads)<<"\n";
            // std::cout<<myOption->payoff(1000000,num_threads)<<"\n";
        }
            
        catch(const std::exception& e){
            std::cerr<<"Error: "<<e.what()<<"\n";
        }
    
    return 0;
}