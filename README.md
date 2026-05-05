# Monte Carlo Options Pricer
### Mathematical Foundations
**Geometric Brownian Motion (GBM)**

This engine simulates underlying asset prices using the Geometric Brownian Motion model. The terminal stock price $S_T$ is calculated using the following equation:

$$S_T = S_0 \exp((r - 0.5\sigma^2)T + \sigma \sqrt{T} Z)$$
- $S_0$: Initial stock price
- $r$: Risk-free interest rate
- $\sigma$: Volatility of the underlying asset
- $T$: Time to expiration (in years)
- $Z$: A standard normal random variable

**Common Random Numbers (CRN) for the Greeks**

Calculating the Greeks (Delta, Gamma, Vega) relies on the finite difference method, which involves "bumping" the initial price or volatility and recalculating the simulated payoff.

To isolate the effect of the bump and eliminate variance noise, this engine strictly implements Common Random Numbers (CRN). The exact same sequence of random normal variables $Z$ used for the base price simulation is perfectly replicated for the bumped simulations. This ensures the calculated derivatives reflect mathematical sensitivity rather than random generation differences.

### Engine Architecture

**RNG State Management**

To guarantee thread safety and mathematically sound CRN, the `std::mt19937` Random Number Generator engine is instantiated once per task inside the worker function, rather than being marked as `thread_local` inside the path generation loop.

- Moving it out of the path generator avoids recreating or pulling the RNG state 250,000 times per task.

- Avoiding `thread_local` prevents critical bugs where threads reused in a pool might inherit a previously mutated RNG state, which would destroy the mathematical alignment required for CRN.


**Algorithmic Micro-Optimizations**

- **Loop-Invariant Code Motion:** Heavy floating-point math (like drift and volatility factors) is pre-calculated outside the main simulation loop. The CPU only processes the dynamic $Z$ variable during the millions of iterations.

- **Loop Unswitching:** String comparisons to check option types (`if tag == "CALL"`) were moved entirely outside the calculation loops. This prevents branch prediction penalties and pipeline disruption across millions of iterations.

### Multithreading Evolution

To process the millions of simulated paths quickly, the engine was designed with data parallelism in mind.

Initially, a *Thread Pool* architecture was implemented. The theoretical reasoning was standard: thread pools eliminate the operating system overhead of constantly destroying and respawning threads. By keeping a set of worker threads alive to constantly check a shared task queue, the engine should theoretically reach peak CPU utilization.

However, real-world benchmarking revealed a different story.

**The Benchmarks: Native Threads vs. Thread Pool**
Both architectures were tested using identical path counts (1,000,000 total per metric) across all three option types.

**Thread-Pooled Architecture Benchmark:**
```text
Price of the European Option - 10.4416

Delta of the European Option - 0.636078

Gamma of the European Option - 0.0189455

Vega of the European Option - 0.374449

************************************************************

Price of the Asian Option - 5.78235

Delta of the Asian Option - 0.589554

Gamma of the Asian Option - 0.0321202

Vega of the Asian Option - 0.217369

************************************************************

Price of the Barrier Option - 8.90646

Delta of the Barrier Option - 0.810122

Gamma of the Barrier Option - 0.193182

Vega of the Barrier Option - 0.146241

************************************************************

Execution time: 57.5374 seconds
```

**Non-Thread-Pooled (Native `std::thread`) Benchmark:**

```text
Price of the European Option - 10.4341

Delta of the European Option - 0.636536

Gamma of the European Option - 0.0185989

Vega of the European Option - 0.374265

************************************************************

Price of the Asian Option - 5.78662

Delta of the Asian Option - 0.589472

Gamma of the Asian Option - 0.032518

Vega of the Asian Option - 0.217556

************************************************************

Price of the Barrier Option - 8.89872

Delta of the Barrier Option - 0.801854

Gamma of the Barrier Option - -0.0343877

Vega of the Barrier Option - 0.134109

************************************************************

Execution time: 54.5154 seconds
```

**Result:** The non-thread-pooled architecture consistently outperformed the thread pool by roughly 3 seconds.

### Why Native Threads Won

In this specific engine, pure, on-demand `std::thread` spawning beat the Thread Pool. This phenomenon comes down to **Task Size vs. Synchronization Overhead.**

1. **Zero Shared State (No Mutex Contention):** A thread pool relies on a shared, central queue. Even highly optimized pools require `std::mutex` locks and condition variables to assign tasks to workers safely. By moving to pure native threads, the engine achieved 100% execution isolation. The CPU cores never have to wait on a lock.

2. **Massive Task Granularity:** The engine only spawns threads exactly 12 times during the entire program execution (3 options $\times$ 4 metrics). Each task is a massive, multi-second calculation chewing through hundreds of thousands of paths.

3. **Negligible OS Overhead:** Letting the operating system spawn a native thread takes a few dozen microseconds. Because there are only 12 massive tasks, this microsecond penalty is effectively zero. The overhead of managing the thread pool's locked queue was mathematically heavier than simply asking the OS for a new thread 12 times.