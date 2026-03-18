#include<iostream>
#include "options.h"
#include<memory>
#include<thread>
#include<stdexcept>

int main() {
        unsigned int num_threads = std::thread::hardware_concurrency();
        num_threads = num_threads == 0? 4 : num_threads;
        try{
            std::unique_ptr<Option> EuOption = std::make_unique<EuropeanOption>(100,100,0.05,0.2,1,"Call"); //unique ptr deletes the object and frees memory automatically when it goes out of scope.
            std::unique_ptr<Option> AOption = std::make_unique<AsianOption>(100,100,0.05,0.2,1,"Call");
            std::unique_ptr<Option> BOption = std::make_unique<BarrierOption>(100,100,0.05,0.2,1,"Call",90);
            double euprice = 0, aprice = 0, bprice = 0; 
            double eudeltaC = 0, eudeltaS = 0; 
            //below we use lambda function to declare threads instead of the direct syntax, to store the return values of the function in the above variables. Direct syntax doesn't allow the threads to store the payoff return values.
            std::thread t1([&EuOption, &euprice, num_threads]() {
                euprice =  EuOption->payoff(1000000, num_threads/4);
            });
            std::thread t2([&AOption, &aprice, num_threads]() {
                aprice =  AOption->payoff(1000000, num_threads/4);
            });
            std::thread t3([&BOption, &bprice, num_threads]() {
                bprice =  BOption->payoff(1000000, num_threads/4);
            });
            std::thread t4([&EuOption, &eudeltaC, num_threads]() {
                eudeltaC =  EuOption->delta2(1000000, num_threads/4);
            });
            t1.join();
            t2.join();
            t3.join();
            t4.join();
            eudeltaS = EuOption->delta(1000000,num_threads);
            std::cout<<"Price of the European Option - "<<euprice<<"\n";
            std::cout<<"Price of the Asian Option - "<<aprice<<"\n";
            std::cout<<"Price of the Barrier Option - "<<bprice<<"\n";
            std::cout<<"Delta of the European Option(clone method) - "<<eudeltaC<<"\n";
            std::cout<<"Delta of the European Option(save and restore method) - "<<eudeltaS<<"\n";
        }
            
        catch(const std::exception& e){
            std::cerr<<"Error: "<<e.what()<<"\n";
        }
    
    return 0;
}