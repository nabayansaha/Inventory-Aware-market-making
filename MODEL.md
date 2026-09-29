# Mathematical Model Specification

Authoritative mathematical specification for the Inventory-Aware Market Making Simulator.
Do not replace these equations with Avellaneda–Stoikov or another market-making model.

Section A: user-supplied microstructure equations.
Section B: project DP completion (authorized formulation completing the incomplete Bellman pieces).

---

## Section A — Microstructure

### 1. Quote definitions

Let \(P_n\) denote the current mid-price. The dealer chooses:

\[
a_n \ge 0, \qquad b_n \le 0
\]

Ask, bid, and spread:

\[
A_n = P_n + a_n
\]

\[
B_n = P_n + b_n
\]

\[
S_n = A_n - B_n = a_n - b_n
\]

### 2. Order arrival intensities

\[
\lambda_n^a = \alpha - \gamma a_n + \omega_n
\]

\[
\lambda_n^b = \beta + \phi b_n + \epsilon_n
\]

where \(\lambda_n^a\) is the intensity of customer BUY orders that hit the ask,
and \(\lambda_n^b\) is the intensity of customer SELL orders that hit the bid.

Admissible controls:

\[
\alpha - \gamma a_n \ge 0
\]

\[
\beta + \phi b_n \ge 0
\]

together with \(a_n \ge 0\), \(b_n \le 0\).

In the live simulator, intensities are floored: \(\lambda \leftarrow \max(\lambda, 0)\).

### 3. Inventory dynamics (DP continuous-fill approximation)

\[
I_n = I_{n-1} + \lambda_n^b - \lambda_n^a
\]

equivalently

\[
I_n = I_{n-1} + \beta + \phi b_n + \epsilon_n - \alpha + \gamma a_n - \omega_n
\]

### 4. Poisson market simulation

DP uses a discrete time grid. The market simulation is event-driven.

\[
\lambda_{\mathrm{tot}} = \lambda_n^a + \lambda_n^b
\]

\[
\Delta t \sim \mathrm{Exponential}(\lambda_{\mathrm{tot}})
\]

\[
\mathbb{P}(\mathrm{ASK\_HIT}) = \frac{\lambda_n^a}{\lambda_{\mathrm{tot}}}, \qquad
\mathbb{P}(\mathrm{BID\_HIT}) = \frac{\lambda_n^b}{\lambda_{\mathrm{tot}}}
\]

### 5. Trade accounting (market simulation, discrete size \(Q\))

ASK_HIT (customer buy hits ask):

\[
\text{trade\_price} = A_n, \quad
I \leftarrow I - Q, \quad
\text{cash} \leftarrow \text{cash} + Q \cdot A_n
\]

BID_HIT (customer sell hits bid):

\[
\text{trade\_price} = B_n, \quad
I \leftarrow I + Q, \quad
\text{cash} \leftarrow \text{cash} - Q \cdot B_n
\]

### 6. Mid-price process

\[
dP_t = \sigma\, dW_t
\]

Between events separated by \(\Delta t\):

\[
P_{t+\Delta t} = P_t + \sigma \sqrt{\Delta t}\, Z, \quad Z \sim \mathcal{N}(0,1)
\]

---

## Section B — Dynamic programming completion

### 7. Period payoff

\[
X_n(a_n,b_n,\omega_n,\epsilon_n)
= a_n \lambda_n^a - b_n \lambda_n^b
= a_n(\alpha - \gamma a_n + \omega_n) - b_n(\beta + \phi b_n + \epsilon_n)
\]

### 8. Utility

\[
U(x) = x
\]

Under linear utility, past cumulative \(\sum_{t<n} \tilde{X}_t\) is action-independent, so the optimizer uses only \(X_n\).

### 9. Bellman equation

Feasible set:

\[
\mathcal{A} = \big\{ (a,b):\; a\ge 0,\; b\le 0,\; \alpha-\gamma a\ge 0,\; \beta+\phi b\ge 0 \big\}
\]

\[
V_n(I_{n-1})
= \max_{(a_n,b_n)\in\mathcal{A}}
\mathbb{E}\big[\, X_n + V_{n+1}(I_n) \,\big]
\]

### 10. Terminal condition

\[
V_{N+1}(I) = -\psi I^2, \quad \psi > 0
\]

Inventory aversion and quote skew arise only from this terminal penalty via backward induction. They are not hard-coded into the quote rules.

### 11. Intensity shocks

\[
\omega_n \sim \mathcal{N}(0, \sigma_\omega^2), \qquad
\epsilon_n \sim \mathcal{N}(0, \sigma_\epsilon^2)
\]

i.i.d., mutually independent, independent across \(n\).

### 12. Expectation

\(\mathbb{E}\) in the Bellman is taken only over \((\omega_n, \epsilon_n)\).
Mid-price Brownian shocks are not inside the DP expectation (state is inventory only).
They evolve in the market simulator for mark-to-market P&L.

Inner expectation uses Gauss–Hermite product quadrature over \((\omega, \epsilon)\).

### 13. Two time mechanisms

| Layer | Time |
|-------|------|
| DP | Discrete grid \(n = 1,\ldots,N\) |
| Market | Continuous / event-driven Poisson |

Do not turn the market simulation into a fixed-step Bernoulli process.
