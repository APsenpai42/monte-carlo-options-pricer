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
#include "threadpool.h"
constexpr const int padding = std::hardware_destructive_interference_size/sizeof(double); //to prevent false sharing, we use padding
class Option
{
    protected:
    double StartingPrice, StrikingPrice, rate, sigma, Time;
    std :: string tag;
    static ThreadPool pool;
    //virtual double generate_path() const= 0; //generate_path has a different return type for each option, and isn't ever being called directly from main. Therefore, we shouldn't make it a base class virtual function.
    virtual void worker(int loops,double &result, unsigned int seed =0) const = 0; //const ensures that the worker is read-only and doesn't modify the members, because during parallel processing, multiple threads will be accessing the same object instance at the same time to read those variables
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
    virtual std::unique_ptr<Option> clone() =0; //we use this clone function for copying the object instead of standard copy constructor to prevent object slicing. Because delta() is a member function of the base Option class, the this pointer only knows it is pointing to an Option. If the actual object is a BarrierOption and we try to do a standard copy, C++ will only copy the base Option attributes and chop off the Barrier parameter.
    double payoff(int loops, int max_threads, unsigned int seed = 0) const //logic for payoff is the same for all options, hence it is better to implement this in base class.
    {//seed value being passed only for the calculation of Greeks. for pricing we will not pass the seed and default value of 0 will be used.
        double total = 0;
        unsigned int num_threads = max_threads;
        std::vector<double> results(padding*num_threads); //padded vector. CPU cores read memory in chunks called Cache Lines. a single cache line can hold 8 of your results at once. Different cores want the same cache line at once. hence we store different results in different cache lines, using padding.
        //std::vector<std::thread> threads(num_threads);
        unsigned int base_loops = loops/num_threads;
        unsigned int remainder = loops%num_threads;
        for(int i = 0;i<num_threads;i++)
        {   
            //std::thread t(&Option::worker, this, base_loops + (i<remainder? 1:0), std::ref(results[padding*i]));
            int thread_loops = base_loops + (i < remainder ? 1 : 0);
            // std::thread t([this, thread_loops, &result = results[padding * i], seed, i]() {
            //     this->worker(thread_loops, result, seed == 0? seed: seed+i);
            // }); //old implementation without threadpooling
            Option::pool.addjob([this, thread_loops, &result = results[padding * i], seed, i]() {
                this->worker(thread_loops, result, seed == 0? seed: seed+i);
            }); //lambda function inside payoff has the same access rights as payoff, so in this case thread isn't the one calling the worker, which might fail otherwise due to class access modifiers.
            //threads[i] = std::move(t);
        }
        Option::pool.wait_finished();
        for(int i = 0;i< num_threads;i++)
        {   
            // threads[i].join();
            total+=results[padding*i];
        }
        double av = total/loops;
        av = av*exp(-this->rate*this->Time);
        return av;
    }
    double delta2(int loops, int max_threads, unsigned int seed = 0)
    {   while(seed == 0 || seed > UINT32_MAX-max_threads)
            seed = std::random_device{}();
        double d;
        double temp = this->StartingPrice;
        double h = 0.001*this->StartingPrice;
        this->StartingPrice-=h;
        double Vd = this->payoff(loops,max_threads, seed);
        this->StartingPrice+=2*h;
        double Vu = this->payoff(loops,max_threads, seed);
        d = (Vu-Vd)/(2*h);
        this->StartingPrice=temp;
        return d;
    } //save and restore method. changes the member variable, hence not thread safe for when spawning multiple threads in main()

    double delta(int loops, int max_threads, unsigned int seed = 0)
    {   while(seed == 0 || seed > UINT32_MAX-max_threads)
            seed = std::random_device{}();
        double d;
        double h = 0.001*this->StartingPrice;
        std::unique_ptr<Option> OptionUp, OptionDown;
        OptionUp = this->clone();
        OptionUp->setStartingPrice(OptionUp->StartingPrice+h);
        OptionDown = this->clone();
        OptionDown->setStartingPrice(OptionDown->StartingPrice-h);
        double Vu = OptionUp->payoff(loops,max_threads, seed);
        double Vd = OptionDown->payoff(loops,max_threads, seed);
        d = (Vu-Vd)/(2*h);
        return d;
    }// clone method. This is thread safe.

    double gamma(int loops, int max_threads, unsigned int seed = 0)
    {
        while(seed == 0 || seed > UINT32_MAX-max_threads)
            seed = std::random_device{}();
        double g;
        double h = 0.001*this->StartingPrice;
        std::unique_ptr<Option> OptionUp, OptionDown;
        OptionUp = this->clone();
        OptionUp->setStartingPrice(OptionUp->StartingPrice+h);
        OptionDown = this->clone();
        OptionDown->setStartingPrice(OptionDown->StartingPrice-h);
        double Vu = OptionUp->payoff(loops,max_threads, seed);
        double Vd = OptionDown->payoff(loops,max_threads, seed);
        double Vb = this->payoff(loops,max_threads, seed);
        g = (Vu - 2*Vb + Vd)/(h*h);
        return g;
    }

    //in both of these, each payoff creates an independent set of 'loops' paths using the RNG. So Vup is created in a different 'universe' than Vdown. so even with 1000000 simulations, there is some error in both. So, we need to ensure that a common random number sequence for both Vup and Vdown. (implemented above)
    double vega(int loops, int max_threads, unsigned int seed = 0)
    {
        while(seed == 0 || seed > UINT32_MAX-max_threads)
            seed = std::random_device{}();
        double v;
        double h = 0.001*this->sigma;
        std::unique_ptr<Option> OptionUp, OptionDown;
        OptionUp = this->clone();
        OptionUp->setVolatility(OptionUp->sigma+h);
        OptionDown = this->clone();
        OptionDown->setVolatility(OptionDown->sigma-h);
        double Vu = OptionUp->payoff(loops,max_threads, seed);
        double Vd = OptionDown->payoff(loops,max_threads, seed);
        v = (Vu-Vd)/(2*h);
        return v/100.0; //dividing by 100 because the v calculates per unit change. since we pass volatility(sigma) as a decimal value instead of % value, the v we get here is the change in price per 1 change in volatility, which in terms of percentage is actually a 100% change. so, we need to divide this by 100. we can avoid this by passing volatility as a percentage value instead, and modifying the final price formula, but the standard practice is to just pass sigma as a decimal as we have done, so that we don't have to divide by 100 in heavy formulas and keep the functions cleaner.
    }
    void setStartingPrice(double S)
    {
        this->StartingPrice = S;
    }
    void setVolatility(double sig)
    {
        this->sigma = sig;
    }
    virtual ~Option(){}
};
class BarrierOption : public Option
{   
    double barrier;
    double generate_path(const std::vector<double> &Z, double t1, double t2) const
    {   
        double daily_price;
        //const double t1 = (rate-0.5*sigma*sigma)*(1.0/252.0), t2 = sigma * sqrt(1.0/252.0);
        // thread_local std::mt19937 mt(std::random_device{}());  
        // thread_local std::normal_distribution<double> nd(0,1);
        // Z=nd(mt);
        daily_price = StartingPrice*exp(t1 + t2 * Z[0]);
        if(daily_price<barrier)
        {
            return 0.0;
        }
        for(int i=1;i<252*Time;i++)
        {
            daily_price = daily_price*exp(t1 + t2 * Z[i]);
            if(daily_price<barrier)
            {
                return 0.0;
            }
        }
        return daily_price;
    }

    void worker(int loops, double &result, unsigned int seed = 0) const override
    {
        double total = 0;
        std::mt19937 mt(seed == 0? std::random_device{}():seed);
        std::normal_distribution<double> nd(0,1);
        std::vector<double> Z(252*Time);
        const double t1 = (rate-0.5*sigma*sigma)*(1.0/252.0), t2 = sigma * sqrt(1.0/252.0);
        // for(int i=0;i<loops;i++)
        // {   
        //     for(int j=0;j<252*Time;j++)
        //         Z[j] = nd(mt);
        //     double ST = this->generate_path(Z,t1,t2);
        //     double payoff;
        //     if(ST==0.0)
        //         payoff = 0.0;            
        //     else if(tag == "CALL")
        //         payoff = std::max(ST-this->StrikingPrice,0.0);
        //     else if(tag == "PUT")
        //         payoff = std::max(this->StrikingPrice-ST,0.0);
        //     total+=payoff;
        // }
        if(tag == "CALL")
        {
            for(int i=0;i<loops;i++)
            {   
                for(int j=0;j<252*Time;j++)
                    Z[j] = nd(mt);
                double ST = this->generate_path(Z,t1,t2);
                double payoff;
                if(ST==0.0)
                    payoff = 0.0;            
                else payoff = std::max(ST-this->StrikingPrice,0.0);
                total+=payoff;
            }
        }   
        else if(tag == "PUT")
        {
            for(int i=0;i<loops;i++)
            {   
                for(int j=0;j<252*Time;j++)
                    Z[j] = nd(mt);
                double ST = this->generate_path(Z,t1,t2);
                double payoff;
                if(ST==0.0)
                    payoff = 0.0;            
                else payoff = std::max(this->StrikingPrice-ST,0.0);
                total+=payoff;
            }
        }
        result = total;
    }
    public:
    BarrierOption(double S0, double K, double r, double s, double T,std::string t, double b)
    : Option(S0, K, r, s, T,t) 
    {barrier = b;}
    std::unique_ptr<Option> clone() override
    {
        std::unique_ptr<Option> Ba = std::make_unique<BarrierOption>(*this);
        return Ba;
    }
    ~BarrierOption() override
    {}
};
class EuropeanOption : public Option
{
    double generate_path(double Z, double t1, double t2) const
    {
        double FinalPrice;
        //thread_local std::mt19937 mt(std::random_device{}());  //thread_local instead of static, works exactly like static but makes a separate copy for each thread. random_device seed ensures that the internal state/sequence is different for each thread. doesn't work with time as the seed.
        //thread_local std::normal_distribution<double> nd(0,1); //we can also not use static/thread_local, but then it's computationally heavy as each loop creates a new state, uses only 1 number and then discards the rest. the sequence can generate 2^19937 - 1 numbers, which is significatly more than the number of loops in each thread
        //Z=nd(mt);
        FinalPrice = StartingPrice*exp(t1 + t2 * Z);
        return FinalPrice;
    }
    void worker(int loops, double &result, unsigned int seed = 0) const override
    {
        double total = 0;
        std::mt19937 mt(seed == 0? std::random_device{}():seed);  //the RNG engines were moved one level up to implement thread pooling in later stages (reusing old threads for new objects). also, the static/thread local have been dropped to avoid the same thread pool bug.
        std::normal_distribution<double> nd(0,1); //the main part was removing static/thread_local so that each task builds a fresh RNG engine. This ensures the engine restarts from the beginning of the sequence using the identical seed, guaranteeing matching paths for Common Random Numbers. However, moving the engine to worker was necessary for performance, as the engine will now be created once per 250000 loops instead of 250000 times (once each loop)
        // we don't move this one level higher (to payoff), because mt19937 is not thread safe, and if 4 threads are reaching 1 RNG engine at the same time in payoff, it will cause data race and scheduling chaos
        const double t1 = (rate-0.5*sigma*sigma)*Time, t2 = sigma * sqrt(Time);
        if(tag == "CALL")
        {
            for(int i=0;i<loops;i++)
            {   double Z = nd(mt);
                double ST = this->generate_path(Z, t1, t2);
                double payoff;
                payoff = std::max(ST-this->StrikingPrice,0.0);
                total+=payoff;
            }
        }
        else if(tag == "PUT")
        {
            for(int i=0;i<loops;i++)
            {   double Z = nd(mt);
                double ST = this->generate_path(Z, t1, t2);
                double payoff;
                payoff = std::max(this->StrikingPrice-ST,0.0);
                total+=payoff;
            }
        }
        result = total;
    }
    public:
    EuropeanOption(double S0, double K, double r, double s, double T,std::string t)
    : Option(S0, K, r, s, T,t) 
    {}
    std::unique_ptr<Option> clone() override
    {
        std::unique_ptr<Option> Eu = std::make_unique<EuropeanOption>(*this);
        return Eu;
    }
    ~EuropeanOption() override
    {}

};

class AsianOption : public Option
{
    std::vector<double> generate_path(const std::vector<double> &Z,double t1,double t2) const
    {
        std::vector<double> daily_prices(252*Time); //to store daily prices.
        //const double t1 = (rate-0.5*sigma*sigma)*(1.0/252.0), t2 = sigma * sqrt(1.0/252.0);
        // thread_local std::mt19937 mt(std::random_device{}());  
        // thread_local std::normal_distribution<double> nd(0,1);
        // Z=nd(mt);
        daily_prices[0] = StartingPrice*exp(t1 + t2 * Z[0]);
        for(int i=1;i<252*Time;i++)
        {
            daily_prices[i] = daily_prices[i-1]*exp(t1 + t2 * Z[i]);
        }
        return daily_prices;
    }
    void worker(int loops, double &result, unsigned int seed =0) const override
    {   
        double total = 0;
        std::mt19937 mt(seed == 0? std::random_device{}():seed);
        std::normal_distribution<double> nd(0,1);
        std::vector<double> Z(252*Time);
        const double t1 = (rate-0.5*sigma*sigma)*(1.0/252.0), t2 = sigma * sqrt(1.0/252.0);
        if(tag == "CALL")
        {
            for(int j=0;j<loops;j++)
            {
                for(int k=0;k<252*Time;k++)
                    Z[k] = nd(mt);
                double average = 0;
                std::vector<double> prices = this->generate_path(Z,t1,t2);
                for(int i =0;i<prices.size();i++)
                {
                    average+=prices[i];
                }
                average/=prices.size();
                double payoff;
                payoff = std::max(average-this->StrikingPrice,0.0);
                total+=payoff;
            }
        }
        else if(tag == "PUT")
        {
            for(int j=0;j<loops;j++)
            {
                for(int k=0;k<252*Time;k++)
                    Z[k] = nd(mt);
                double average = 0;
                std::vector<double> prices = this->generate_path(Z,t1,t2);
                for(int i =0;i<prices.size();i++)
                {
                    average+=prices[i];
                }
                average/=prices.size();
                double payoff;
                payoff = std::max(this->StrikingPrice-average,0.0);
                total+=payoff;
            }
        }
        result = total;
    }
    public:
    AsianOption(double S0, double K, double r, double s, double T,std::string t)
    : Option(S0, K, r, s, T,t) 
    {}
    std::unique_ptr<Option> clone() override
    {
        std::unique_ptr<Option> As = std::make_unique<AsianOption>(*this);
        return As;
    }
    ~AsianOption() override
    {}
};