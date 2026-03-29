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
            double eudelta = 0, eugamma= 0, euvega =0, adelta = 0, agamma = 0, avega= 0, bdelta = 0, bgamma = 0, bvega = 0; 
            //below we use lambda function to declare threads instead of the direct syntax, to store the return values of the function in the above variables. Direct syntax doesn't allow the threads to store the payoff return values.
            std::thread t1([&EuOption, &euprice, num_threads]() {
                euprice =  EuOption->payoff(1000000, num_threads/4);
            });
            std::thread t2([&EuOption, &eudelta, num_threads]() {
                eudelta =  EuOption->delta(1000000, num_threads/4);
            });
            std::thread t3([&EuOption, &eugamma, num_threads]() {
                eugamma =  EuOption->gamma(1000000, num_threads/4);
            });
            std::thread t4([&EuOption, &euvega, num_threads]() {
                euvega =  EuOption->vega(1000000, num_threads/4);
            });
            t1.join();
            t2.join();
            t3.join();
            t4.join();
            std::cout<<"Price of the European Option - "<<euprice<<"\n";
            std::cout<<"Delta of the European Option - "<<eudelta<<"\n";
            std::cout<<"Gamma of the European Option - "<<eugamma<<"\n";
            std::cout<<"Vega of the European Option - "<<euvega<<"\n";
            std::cout<<"************************************************************\n";

            t1 = std::thread([&AOption, &aprice, num_threads]() {
                aprice =  AOption->payoff(1000000, num_threads/4);
            });
            t2 = std:: thread([&AOption, &adelta, num_threads]() {
                adelta =  AOption->delta(1000000, num_threads/4);
            });
            t3 = std:: thread([&AOption, &agamma, num_threads]() {
                agamma =  AOption->gamma(1000000, num_threads/4);
            });
            t4 = std:: thread([&AOption, &avega, num_threads]() {
                avega =  AOption->vega(1000000, num_threads/4);
            });
            t1.join();
            t2.join();
            t3.join();
            t4.join();
            std::cout<<"Price of the Asian Option - "<<aprice<<"\n";
            std::cout<<"Delta of the Asian Option - "<<adelta<<"\n";
            std::cout<<"Gamma of the Asian Option - "<<agamma<<"\n";
            std::cout<<"Vega of the Asian Option - "<<avega<<"\n";
            std::cout<<"************************************************************\n";

            t1 = std::thread([&BOption, &bprice, num_threads]() {
                bprice =  BOption->payoff(1000000, num_threads/4);
            });
            t2 = std:: thread([&BOption, &bdelta, num_threads]() {
                bdelta =  BOption->delta(1000000, num_threads/4);
            });
            t3 = std:: thread([&BOption, &bgamma, num_threads]() {
                bgamma =  BOption->gamma(1000000, num_threads/4);
            });
            t4 = std:: thread([&BOption, &bvega, num_threads]() {
                bvega =  BOption->vega(1000000, num_threads/4);
            });
            t1.join();
            t2.join();
            t3.join();
            t4.join();
            std::cout<<"Price of the Barrier Option - "<<bprice<<"\n";
            std::cout<<"Delta of the Barrier Option - "<<bdelta<<"\n";
            std::cout<<"Gamma of the Barrier Option - "<<bgamma<<"\n";
            std::cout<<"Vega of the Barrier Option - "<<bvega<<"\n";
            std::cout<<"************************************************************\n";
            
        }
            
        catch(const std::exception& e){
            std::cerr<<"Error: "<<e.what()<<"\n";
        }
    
    return 0;
}