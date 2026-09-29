# Model → Code Mapping

Maps every equation in [MODEL.md](../MODEL.md) to C++ symbols.
Do not implement equations that are not listed here.

| Eq | Mathematics | Class / namespace | Function / member | Variables |
|----|-------------|------------------|-------------------|-----------|
| A1 | \(A_n = P_n + a_n\) | `mm::Model` | `askPrice` | `mid`, `askOffset` (`a`) |
| A1 | \(B_n = P_n + b_n\) | `mm::Model` | `bidPrice` | `mid`, `bidOffset` (`b`) |
| A1 | \(S_n = a_n - b_n\) | `mm::Model` | `spread` | `askOffset`, `bidOffset` |
| A2 | \(\lambda^a = \alpha - \gamma a + \omega\) | `mm::Model` | `askIntensity` | `alpha`, `gamma`, `a`, `omega` |
| A2 | \(\lambda^b = \beta + \phi b + \epsilon\) | `mm::Model` | `bidIntensity` | `beta`, `phi`, `b`, `epsilon` |
| A2 | \(\alpha - \gamma a \ge 0\), \(\beta + \phi b \ge 0\), \(a\ge0\), \(b\le0\) | `mm::Model` | `feasible` | params + controls |
| A2 | \(\lambda \leftarrow \max(\lambda,0)\) | `mm::Model` | `askIntensity` / `bidIntensity` | floor |
| A3 | \(I_n = I_{n-1} + \lambda^b - \lambda^a\) | `mm::Model` | `nextInventory` | `inventory`, intensities |
| A4 | \(\Delta t \sim \mathrm{Exp}(\lambda_{\mathrm{tot}})\) | `mm::MarketSimulator` | `sampleWaitingTime` | `lambdaAsk`, `lambdaBid` |
| A4 | \(\mathbb{P}(\mathrm{ASK\_HIT})=\lambda^a/\lambda_{\mathrm{tot}}\) | `mm::MarketSimulator` | `sampleHitSide` | intensities |
| A5 | ASK_HIT accounting | `mm::MarketSimulator` | `executeAskHit` | `Q`, `ask`, cash, inventory |
| A5 | BID_HIT accounting | `mm::MarketSimulator` | `executeBidHit` | `Q`, `bid`, cash, inventory |
| A6 | \(P \leftarrow P + \sigma\sqrt{\Delta t}\,Z\) | `mm::ArithmeticBrownianMotion` | `evolve` | `sigma`, `dt`, `Z` |
| B7 | \(X_n = a\lambda^a - b\lambda^b\) | `mm::Model` | `periodPayoff` | controls + intensities |
| B8 | \(U(x)=x\) | `mm::IdentityUtility` | `evaluate` | wealth / payoff |
| B9 | Bellman recursion | `mm::DPSolver` | `solve` | `V`, `optimalA`, `optimalB` |
| B9 | Feasible set \(\mathcal{A}\) | `mm::Model` | `feasible` / control grid | `aMin`…`aMax`, `bMin`…`bMax` |
| B10 | \(V_{N+1}(I)=-\psi I^2\) | `mm::DPSolver` | `terminalValue` | `psi`, `inventory` |
| B11 | \(\omega\sim\mathcal{N}(0,\sigma_\omega^2)\), \(\epsilon\sim\mathcal{N}(0,\sigma_\epsilon^2)\) | `mm::DPSolver` / `mm::MarketSimulator` | quadrature / `std::normal_distribution` | `sigmaOmega`, `sigmaEpsilon` |
| B12 | \(\mathbb{E}\) over \((\omega,\epsilon)\) only | `mm::DPSolver` | `expectedObjective` | GH nodes/weights |
| — | Policy query \(a^*(t,I)\), \(b^*(t,I)\) | `mm::Policy` | `optimalAskOffset`, `optimalBidOffset`, `value` | time, inventory |
| — | Dealer state | `mm::MarketMakerState` | struct fields | cash, inventory, PnL, quotes, λ |
| — | Events | `mm::MarketEvent` | struct fields | type, prices, sizes, PnL |
| — | Wealth \(=\mathrm{cash}+I\cdot P\) | `mm::MarketMakerState` | `updatePnL` | cash, inventory, mid |

## Module boundaries

| Module | Responsibility |
|--------|----------------|
| `Model` | Intensities, constraints, quotes, period payoff, inventory transition |
| `Utility` | \(U(x)=x\) |
| `PriceProcess` | Mid-price evolution |
| `DPSolver` / `Policy` | Backward induction and policy interpolation |
| `MarketSimulator` | Event-driven Poisson market + accounting |
| `MarketMakerState` | Live dealer state |
| `UI` | Visualization only; no model logic |
