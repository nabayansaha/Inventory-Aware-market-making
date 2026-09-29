#!/usr/bin/env python3
"""Live trader vs the C++ market maker TCP bridge.

Supports:
  - default built-in rules
  - strategies pushed from the terminal (set_strategy)
  - --strategy path/to/file.py loaded at startup

Strategy API:
  def decide(book, history, state) -> list[dict]
"""

from __future__ import annotations

import argparse
import json
import socket
import sys
import time
import traceback
import uuid
from typing import Any, Callable


DecideFn = Callable[[dict[str, Any], list[dict[str, Any]], dict[str, Any]], list[dict[str, Any]]]


def builtin_decide(book: dict[str, Any], history: list[dict[str, Any]], state: dict[str, Any]) -> list[dict[str, Any]]:
    """Momentum on previous bid/ask + book width."""
    orders: list[dict[str, Any]] = []
    mid = float(book["mid"])
    bid = float(book["bid"])
    ask = float(book["ask"])
    spread = float(book.get("spread", ask - bid))
    size = float(state.get("size", 1.0))
    tick = float(state.get("tick", 0.05))
    edge = float(state.get("edge", 0.15))
    min_spread = float(state.get("min_spread", 0.8))

    if len(history) >= 2:
        prev = history[-2]
        prev_bid = float(prev["bid"])
        prev_ask = float(prev["ask"])
        prev_mid = float(prev["mid"])
        bid_up = bid - prev_bid
        ask_up = ask - prev_ask
        mid_up = mid - prev_mid

        if bid_up > tick * 0.5 and mid_up > 0 and ask <= mid + edge:
            orders.append({"order_type": "MARKET", "side": "BUY", "size": size})
            return orders
        if ask_up < -tick * 0.5 and mid_up < 0 and bid >= mid - edge:
            orders.append({"order_type": "MARKET", "side": "SELL", "size": size})
            return orders

    if ask < mid - edge:
        orders.append({"order_type": "MARKET", "side": "BUY", "size": size})
    elif bid > mid + edge:
        orders.append({"order_type": "MARKET", "side": "SELL", "size": size})
    elif spread >= min_spread:
        orders.append({"order_type": "LIMIT", "side": "BUY", "price": bid + tick, "size": size})
        orders.append({"order_type": "LIMIT", "side": "SELL", "price": ask - tick, "size": size})
    return orders


def load_strategy_source(source: str) -> DecideFn:
    ns: dict[str, Any] = {}
    exec(compile(source, "<strategy>", "exec"), ns, ns)
    fn = ns.get("decide")
    if not callable(fn):
        raise ValueError("strategy must define decide(book, history, state)")
    return fn  # type: ignore[return-value]


def load_strategy_file(path: str) -> tuple[str, DecideFn]:
    with open(path, "r", encoding="utf-8") as f:
        source = f.read()
    return source, load_strategy_source(source)


class TraderClient:
    def __init__(self, host: str, port: int, args: argparse.Namespace) -> None:
        self.host = host
        self.port = port
        self.args = args
        self.sock: socket.socket | None = None
        self.buf = ""
        self.cash = 0.0
        self.inventory = 0.0
        self.last_book: dict[str, Any] | None = None
        self.history: list[dict[str, Any]] = []
        self.order_seq = 0
        self.last_action_t = 0.0
        self.cooldown_s = 0.35
        self.decide: DecideFn = builtin_decide
        self.strategy_name = "builtin"
        if args.strategy:
            src, fn = load_strategy_file(args.strategy)
            self.decide = fn
            self.strategy_name = args.strategy
            print(f"loaded strategy file: {args.strategy} ({len(src)} bytes)", flush=True)

    def connect(self) -> None:
        self.sock = socket.create_connection((self.host, self.port), timeout=5.0)
        self.sock.settimeout(0.2)
        print(f"connected to {self.host}:{self.port} strategy={self.strategy_name}", flush=True)

    def close(self) -> None:
        if self.sock is not None:
            self.sock.close()
            self.sock = None

    def _recv_lines(self) -> list[str]:
        assert self.sock is not None
        lines: list[str] = []
        try:
            data = self.sock.recv(65536)
            if not data:
                raise ConnectionError("server closed")
            self.buf += data.decode("utf-8", errors="replace")
        except socket.timeout:
            pass
        while True:
            idx = self.buf.find("\n")
            if idx < 0:
                break
            line = self.buf[:idx].strip()
            self.buf = self.buf[idx + 1 :]
            if line:
                lines.append(line)
        return lines

    def send(self, msg: dict[str, Any]) -> None:
        assert self.sock is not None
        self.sock.sendall((json.dumps(msg) + "\n").encode("utf-8"))

    def new_id(self, prefix: str) -> str:
        self.order_seq += 1
        return f"{prefix}-{self.order_seq}-{uuid.uuid4().hex[:6]}"

    def place(self, order: dict[str, Any]) -> str:
        ot = str(order.get("order_type", "MARKET")).upper()
        side = str(order.get("side", "BUY")).upper()
        size = float(order.get("size", self.args.size))
        oid = self.new_id(ot[:3].lower())
        msg: dict[str, Any] = {
            "type": "place",
            "id": oid,
            "order_type": ot,
            "side": side,
            "size": size,
        }
        if ot == "LIMIT":
            msg["price"] = float(order["price"])
        if ot == "STOP":
            msg["stop"] = float(order["stop"])
        self.send(msg)
        return oid

    def on_message(self, msg: dict[str, Any]) -> None:
        typ = msg.get("type")
        if typ == "set_strategy":
            source = msg.get("source", "")
            try:
                self.decide = load_strategy_source(source)
                self.strategy_name = "terminal_upload"
                print(f"loaded strategy from terminal ({len(source)} bytes)", flush=True)
                self.send({"type": "strategy_ack", "ok": True, "name": self.strategy_name})
            except Exception as exc:  # noqa: BLE001
                print(f"strategy load failed: {exc}", flush=True)
                traceback.print_exc()
                self.send({"type": "strategy_ack", "ok": False, "error": str(exc)})
            return

        if typ == "book":
            snap = {
                "t": msg.get("t"),
                "mid": msg.get("mid"),
                "bid": msg.get("bid"),
                "ask": msg.get("ask"),
                "spread": msg.get("spread"),
                "bids": msg.get("bids"),
                "asks": msg.get("asks"),
                "inventory": msg.get("inventory"),
            }
            self.history.append(snap)
            if len(self.history) > 256:
                self.history = self.history[-256:]
            self.last_book = msg
            self.cash = float(msg.get("trader_cash", self.cash))
            self.inventory = float(msg.get("trader_inventory", self.inventory))
            self.maybe_trade(msg)
            return

        if typ == "fill":
            side = msg.get("side")
            px = float(msg["price"])
            sz = float(msg["size"])
            print(f"FILL {side} {sz} @ {px:.4f}", flush=True)
            if side == "BUY" and self.args.stop_ticks > 0:
                stop = px - self.args.stop_ticks * self.args.tick
                self.place({"order_type": "STOP", "side": "SELL", "stop": stop, "size": sz})
                print(f"armed SELL stop @ {stop:.4f}", flush=True)
            elif side == "SELL" and self.args.stop_ticks > 0:
                stop = px + self.args.stop_ticks * self.args.tick
                self.place({"order_type": "STOP", "side": "BUY", "stop": stop, "size": sz})
                print(f"armed BUY stop @ {stop:.4f}", flush=True)
            return

        if typ == "reject":
            print(f"REJECT {msg}", flush=True)
        elif typ == "hello":
            print(f"server: {msg.get('msg')}", flush=True)

    def maybe_trade(self, book: dict[str, Any]) -> None:
        now = time.time()
        if now - self.last_action_t < self.cooldown_s:
            return
        state = {
            "cash": self.cash,
            "inventory": self.inventory,
            "size": self.args.size,
            "tick": self.args.tick,
            "edge": self.args.edge,
            "min_spread": self.args.min_spread,
        }
        try:
            orders = self.decide(book, self.history, state) or []
        except Exception as exc:  # noqa: BLE001
            print(f"decide() error: {exc}", flush=True)
            traceback.print_exc()
            return
        if not orders:
            return
        for order in orders[:4]:
            oid = self.place(order)
            print(
                f"ORDER {oid} {order.get('order_type')} {order.get('side')} {order}",
                flush=True,
            )
        self.last_action_t = now

    def run(self) -> None:
        self.connect()
        last_status = 0.0
        try:
            while True:
                for line in self._recv_lines():
                    try:
                        msg = json.loads(line)
                    except json.JSONDecodeError:
                        continue
                    self.on_message(msg)
                now = time.time()
                if now - last_status > 2.0 and self.last_book is not None:
                    pnl = float(self.last_book.get("trader_pnl", 0.0))
                    print(
                        f"status strategy={self.strategy_name} cash={self.cash:.2f} "
                        f"inv={self.inventory:.2f} pnl={pnl:.2f} "
                        f"mid={float(self.last_book['mid']):.4f}",
                        flush=True,
                    )
                    last_status = now
                time.sleep(0.05)
        except (KeyboardInterrupt, ConnectionError) as exc:
            print(f"exit: {exc}", flush=True)
        finally:
            self.close()


def main() -> int:
    p = argparse.ArgumentParser(description="Live trader vs market maker")
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--port", type=int, default=8765)
    p.add_argument("--size", type=float, default=1.0)
    p.add_argument("--edge", type=float, default=0.15)
    p.add_argument("--min-spread", type=float, default=0.8)
    p.add_argument("--tick", type=float, default=0.05)
    p.add_argument("--stop-ticks", type=float, default=4.0)
    p.add_argument("--strategy", default="", help="optional .py file with decide()")
    args = p.parse_args()

    client = TraderClient(args.host, args.port, args)
    try:
        client.run()
    except OSError as exc:
        print(f"connect failed: {exc}", file=sys.stderr)
        print("Start market_maker and Enable Trader Port first.", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
