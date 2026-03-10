#pragma once

#include<cmath>
#include<random>
#include<string>
#include<algorithm>
#include<memory>
#include<thread>
#include<new>
#include<vector>
#include<stdexcept>
constexpr const int padding = std::hardware_destructive_interference_size/sizeof(double); //to prevent false sharing, we use padding
class Option
{
    protected:
    double StartingPrice, StrikingPrice, rate, sigma, Time;
    std :: string tag;
    //virtual double generate_path() const= 0; //generate_path has a different return type for each option, and isn't ever being called directly from main. Therefore, we shouldn't make it a base class virtual function.
    virtual void worker(int loops,double &result) const = 0; //const ensures that the worker is read-only and doesn't modify the members, because during parallel processing, multiple threads will be accessing the same object instance at the same time to read those variables
    public:
    Option(double S0, double K, double r, double s, double T, std::string t)
    : StartingPrice(S0), StrikingPrice(K), rate(r), sigma(s), Time(T), tag(t)
    {
        std::transform(tag.begin(), tag.end(), tag.begin(), 
                       [](unsigned char c) { return std::toupper(c); });

        if (tag != "CALL" && tag != "PUT") {
            throw std::invalid_argument("Invalid Type: Must be Call or Put");
        }
    }
    double payoff(int loops, int max_threads) //logic for payoff is the same for all options, hence it is better to implement this in base class.
    {
        double total = 0;
        unsigned int num_threads = max_threads;
        std::vector<double> results(padding*num_threads); //padded vector. CPU cores read memory in chunks called Cache Lines. a single cache line can hold 8 of your results at once. Different cores want the same cache line at once. hence we store different results in different cache lines, using padding.
        std::vector<std::thread> threads(num_threads);
        unsigned int base_loops = loops/num_threads;
        unsigned int remainder = loops%num_threads;
        for(int i = 0;i<num_threads;i++)
        {   
            //std::thread t(&Option::worker, this, base_loops + (i<remainder? 1:0), std::ref(results[padding*i]));
            int thread_loops = base_loops + (i < remainder ? 1 : 0);
            std::thread t([this, thread_loops, &result = results[padding * i]]() {
                this->worker(thread_loops, result);
            }); //lambda function inside payoff has the same access rights as payoff, so in this case thread isn't the one calling the worker, which might fail otherwise due to class access modifiers.
            threads[i] = std::move(t);
        }
        for(int i = 0;i< num_threads;i++)
        {   
            threads[i].join();
            total+=results[padding*i];
        }
        double av = total/loops;
        av = av*exp(-this->rate*this->Time);
        return av;
    }
    virtual ~Option(){}
};
class BarrierOption : public Option
{   
    double barrier;
    double generate_path() const
    {   
        double daily_price;
        double Z;
        const double t1 = (rate-0.5*sigma*sigma)*(1.0/252.0), t2 = sigma * sqrt(1.0/252.0);
        thread_local std::mt19937 mt(std::random_device{}());  
        thread_local std::normal_distribution<double> nd(0,1);
        Z=nd(mt);
        daily_price = StartingPrice*exp(t1 + t2 * Z);
        if(daily_price<barrier)
        {
            return 0.0;
        }
        for(int i=1;i<252*Time;i++)
        {
            Z=nd(mt);
            daily_price = daily_price*exp(t1 + t2 * Z);
            if(daily_price<barrier)
            {
                return 0.0;
            }
        }
        return daily_price;
    }

    void worker(int loops, double &result) const override
    {
        double total = 0;
        for(int i=0;i<loops;i++)
        {
            double ST = this->generate_path();
            double payoff;
            if(ST==0.0)
                payoff = 0.0;            
            else if(tag == "CALL")
                payoff = std::max(ST-this->StrikingPrice,0.0);
            else if(tag == "PUT")
                payoff = std::max(this->StrikingPrice-ST,0.0);
            total+=payoff;
        }
        result = total;
    }
    public:
    BarrierOption(double S0, double K, double r, double s, double T,std::string t, double b)
    : Option(S0, K, r, s, T,t) 
    {barrier = b;}
    ~BarrierOption() override
    {}
};
class EuropeanOption : public Option
{
    double generate_path() const
    {
        double Z,FinalPrice;
        thread_local std::mt19937 mt(std::random_device{}());  //thread_local instead of static, works exactly like static but makes a separate copy for each thread. random_device seed ensures that the internal state/sequence is different for each thread. doesn't work with time as the seed.
        thread_local std::normal_distribution<double> nd(0,1); //we can also not use static/thread_local, but then it's computationally heavy as each loop creates a new state, uses only 1 number and then discards the rest. the sequence can generate 2^19937 - 1 numbers, which is significatly more than the number of loops in each thread
        Z=nd(mt);
        FinalPrice = StartingPrice*exp((rate-0.5*sigma*sigma)*Time + sigma * sqrt(Time) * Z);
        return FinalPrice;
    }
    void worker(int loops, double &result) const override
    {
        double total = 0;
        for(int i=0;i<loops;i++)
        {
            double ST = this->generate_path();
            double payoff;
            
            if(tag == "CALL")
                payoff = std::max(ST-this->StrikingPrice,0.0);
            else if(tag == "PUT")
                payoff = std::max(this->StrikingPrice-ST,0.0);
            total+=payoff;
        }
        result = total;
    }
    public:
    EuropeanOption(double S0, double K, double r, double s, double T,std::string t)
    : Option(S0, K, r, s, T,t) 
    {}
    ~EuropeanOption() override
    {}

};

class AsianOption : public Option
{
    std::vector<double> generate_path() const
    {
        std::vector<double> daily_prices(252*Time); //to store daily prices.
        double Z;
        const double t1 = (rate-0.5*sigma*sigma)*(1.0/252.0), t2 = sigma * sqrt(1.0/252.0);
        thread_local std::mt19937 mt(std::random_device{}());  
        thread_local std::normal_distribution<double> nd(0,1);
        Z=nd(mt);
        daily_prices[0] = StartingPrice*exp(t1 + t2 * Z);
        for(int i=1;i<252*Time;i++)
        {
            Z=nd(mt);
            daily_prices[i] = daily_prices[i-1]*exp(t1 + t2 * Z);
        }
        return daily_prices;
    }
    void worker(int loops, double &result) const override
    {   
        double total = 0;
        for(int j=0;j<loops;j++)
        {
            double average = 0;
            std::vector<double> prices = this->generate_path();
            for(int i =0;i<prices.size();i++)
            {
                average+=prices[i];
            }
            average/=prices.size();
            double payoff;
            if(tag == "CALL")
                payoff = std::max(average-this->StrikingPrice,0.0);
            else if(tag == "PUT")
                payoff = std::max(this->StrikingPrice-average,0.0);
            total+=payoff;
        }
        result = total;
    }
    public:
    AsianOption(double S0, double K, double r, double s, double T,std::string t)
    : Option(S0, K, r, s, T,t) 
    {}
    ~AsianOption() override
    {}
};