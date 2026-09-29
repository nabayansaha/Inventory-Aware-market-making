# Example uploaded strategy for the live Python trader.
# Required: define decide(book, history, state) -> list[dict]
#
# book: latest snapshot (mid, bid, ask, spread, bids, asks, inventory, ...)
# history: prior books (oldest -> newest), each with mid/bid/ask/spread
# state: {"cash", "inventory", "size", "tick"}
#
# Return orders like:
#   {"order_type":"MARKET","side":"BUY","size":1}
#   {"order_type":"LIMIT","side":"SELL","price":100.2,"size":1}
#   {"order_type":"STOP","side":"SELL","stop":99.5,"size":1}

def decide(book, history, state):
    orders = []
    mid = float(book["mid"])
    bid = float(book["bid"])
    ask = float(book["ask"])
    spread = float(book.get("spread", ask - bid))
    size = float(state.get("size", 1.0))
    tick = float(state.get("tick", 0.05))
    inv = float(state.get("inventory", 0.0))

    # Need at least one prior quote to measure bid/ask momentum.
    if len(history) < 2:
        return orders

    prev = history[-2]
    prev_bid = float(prev["bid"])
    prev_ask = float(prev["ask"])
    prev_mid = float(prev["mid"])

    bid_up = bid - prev_bid
    ask_up = ask - prev_ask
    mid_up = mid - prev_mid

    # Lifted offers / firming bids -> lean long with market or limit.
    if bid_up > tick * 0.5 and mid_up > 0:
        if ask < mid + tick:
            orders.append({"order_type": "MARKET", "side": "BUY", "size": size})
        else:
            orders.append(
                {
                    "order_type": "LIMIT",
                    "side": "BUY",
                    "price": bid + tick,
                    "size": size,
                }
            )

    # Offered lower / mid fading -> lean short.
    if ask_up < -tick * 0.5 and mid_up < 0:
        if bid > mid - tick:
            orders.append({"order_type": "MARKET", "side": "SELL", "size": size})
        else:
            orders.append(
                {
                    "order_type": "LIMIT",
                    "side": "SELL",
                    "price": ask - tick,
                    "size": size,
                }
            )

    # Wide book: join inside using ladder.
    if spread >= 0.8 and not orders:
        depth_bid = book.get("bids") or []
        depth_ask = book.get("asks") or []
        if depth_bid and depth_ask:
            orders.append(
                {
                    "order_type": "LIMIT",
                    "side": "BUY",
                    "price": bid + tick,
                    "size": size,
                }
            )
            orders.append(
                {
                    "order_type": "LIMIT",
                    "side": "SELL",
                    "price": ask - tick,
                    "size": size,
                }
            )

    # Inventory guard: if already long/short, prefer flattening.
    if inv >= 3 and mid_up < 0:
        orders = [{"order_type": "MARKET", "side": "SELL", "size": size}]
    elif inv <= -3 and mid_up > 0:
        orders = [{"order_type": "MARKET", "side": "BUY", "size": size}]

    return orders
