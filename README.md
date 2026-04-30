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
