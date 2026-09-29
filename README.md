# Inventory-Aware Market Making Simulator

C++20 event-driven dealer simulator with a dynamic-programming quote policy
and a Dear ImGui / ImPlot trading terminal.

Authoritative mathematics: [MODEL.md](MODEL.md).  
Equation → code map: [docs/model_mapping.md](docs/model_mapping.md).

This is **not** Avellaneda–Stoikov. Inventory-dependent skew comes only from
backward induction on the project Bellman equation.

---

## 1. What it does

The application posts bid/ask quotes from a DP policy, generates customer
market orders via a Poisson process, executes against the dealer, updates
cash/inventory/P&L, evolves the mid-price, and recomputes quotes in a live
terminal loop.

```
DP policy a*(t,I), b*(t,I)
        ↓
Bid = Mid + b*,  Ask = Mid + a*
        ↓
λ_ask, λ_bid  →  Δt ~ Exp(λ_ask + λ_bid)
        ↓
Evolve mid  →  ASK_HIT or BID_HIT
        ↓
Update cash, inventory, P&L  →  repeat
```

---

## 2. Mathematical model

### Quotes

\[
A_n = P_n + a_n,\quad B_n = P_n + b_n,\quad a_n \ge 0,\; b_n \le 0
\]

\[
S_n = a_n - b_n
\]

### Intensities

\[
\lambda_n^a = \alpha - \gamma a_n + \omega_n
\]

\[
\lambda_n^b = \beta + \phi b_n + \epsilon_n
\]

Constraints: \(\alpha - \gamma a_n \ge 0\), \(\beta + \phi b_n \ge 0\).

### Inventory (DP continuous-fill approximation)

\[
I_n = I_{n-1} + \lambda_n^b - \lambda_n^a
\]

### Period payoff and Bellman

\[
X_n = a_n\lambda_n^a - b_n\lambda_n^b
\]

\[
U(x) = x
\]

\[
V_n(I) = \max_{(a,b)\in\mathcal{A}} \mathbb{E}_{\omega,\epsilon}\big[X_n + V_{n+1}(I_n)\big]
\]

\[
V_{N+1}(I) = -\psi I^2
\]

Shocks: \(\omega_n,\epsilon_n \sim \mathcal{N}(0,\sigma^2)\) i.i.d. Expectation in the
Bellman is over intensity shocks only.

### Market simulation

\[
\Delta t \sim \mathrm{Exp}(\lambda^a+\lambda^b),\quad
\mathbb{P}(\mathrm{ASK\_HIT})=\lambda^a/(\lambda^a+\lambda^b)
\]

Trade size \(Q\). Mid-price: \(dP_t = \sigma\,dW_t\).

---

## 3–8. Formulation summary

| Topic | Implementation |
|-------|----------------|
| DP | `DPSolver` backward induction + grid search |
| Quotes | `Model::askPrice` / `bidPrice` |
| Poisson | `MarketSimulator::sampleWaitingTime` |
| Inventory | discrete \(Q\) fills in sim; continuous \(\lambda\) in DP |
| Price | `ArithmeticBrownianMotion` |
| P&L | wealth = cash + inventory × mid |

---

## 9. Architecture

| Module | Role |
|--------|------|
| `src/model` | Intensities, utility, price process |
| `src/dp` | Value function and optimal policies |
| `src/simulation` | Event engine, state, events |
| `src/analysis` | Statistics, CSV, Monte Carlo |
| `src/ui` | Trading terminal |

---

## 10. Build (macOS)

Dependencies: CMake ≥ 3.20, C++20 compiler, GLFW, OpenGL.

```bash
brew install cmake glfw

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/opt/homebrew
cmake --build build -j
```

Dear ImGui, ImPlot, and Catch2 are fetched automatically via CMake FetchContent.

### Linux

```bash
sudo apt install cmake libglfw3-dev libgl1-mesa-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### Windows

Install CMake, a C++20 toolchain, and GLFW. Configure with the GLFW prefix
path, then build the `market_maker` target.

---

## 11. Run

```bash
# Trading terminal (default when UI is built)
./build/market_maker

# Headless DP + simulation + CSV
./build/market_maker --headless --csv simulation.csv --seed 42

# Tests
ctest --test-dir build --output-on-failure
```

---

## 12. Parameters

Edit the sidebar, then press **APPLY / RESET** (re-solves the DP on a worker
thread). Important knobs:

- `alpha`, `beta`, `gamma`, `phi` — intensity intercepts/slopes  
- `psi` — terminal inventory penalty  
- `sigma` — mid-price volatility  
- `sigma omega` / `sigma epsilon` — intensity shock vols  
- inventory grid and DP steps / resolution (Fast / Normal / High)  
- random seed (replay with **REPLAY**)

Controls: **START**, **PAUSE**, **RESET**, **REPLAY**, speed `0.25x`…`100x`.

Tabs: **Live**, **Monte Carlo**, **Summary** (end-of-horizon report).

---

## 13. Live Python trader

The terminal can open a localhost TCP JSON bridge so an external Python trader
receives the full synthetic order book and submits **MARKET**, **LIMIT**, or
**STOP** orders against the dealer.

1. Start the UI: `./build/market_maker`
2. Click **Enable Trader Port** (default `8765`)
3. Open the **Trader** tab, load a strategy file (or edit inline), click **Push to Bot**
4. Press **START** on the simulation
5. In another terminal:

```bash
python3 traders/live_trader.py --host 127.0.0.1 --port 8765
# or with a file:
python3 traders/live_trader.py --strategy traders/strategies/momentum_book.py
```

### Strategy API

Uploaded / pushed strategies must define:

```python
def decide(book, history, state):
    # history: prior bid/ask/mid snapshots
    # return list of {"order_type","side","size",...}
    return []
```

Example: `traders/strategies/momentum_book.py` uses previous bid/ask moves and book width.

The **Orders** tab lists live Python LIMIT/STOP orders and recent Poisson market-flow hits.

Useful flags: `--edge`, `--min-spread`, `--size`, `--stop-ticks`.

Protocol is line-delimited JSON (`book`, `place`, `cancel`, `fill`, `set_strategy`,
`ack`, `reject`). External fills appear on the event tape with an `EXT` tag.

---

## License

Project code is provided as-is for research and education.
