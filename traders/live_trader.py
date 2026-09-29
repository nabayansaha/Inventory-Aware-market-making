#!/usr/bin/env python3
"""Live adversarial trader against the C++ market maker TCP bridge.

Usage:
  1. Start ./build/market_maker
  2. Click Enable Trader Port (default 8765)
  3. python3 traders/live_trader.py --port 8765
"""

from __future__ import annotations

import argparse
import json
import socket
import sys
import time
import uuid
from typing import Any


class TraderClient:
    def __init__(self, host: str, port: int) -> None:
        self.host = host
        self.port = port
        self.sock: socket.socket | None = None
        self.buf = ""
        self.cash = 0.0
        self.inventory = 0.0
        self.last_book: dict[str, Any] | None = None
        self.open_stops: dict[str, str] = {}
        self.order_seq = 0
        self.last_action_t = 0.0
        self.cooldown_s = 0.35

    def connect(self) -> None:
        self.sock = socket.create_connection((self.host, self.port), timeout=5.0)
        self.sock.settimeout(0.2)
        print(f"connected to {self.host}:{self.port}", flush=True)

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
        payload = json.dumps(msg) + "\n"
        self.sock.sendall(payload.encode("utf-8"))

    def new_id(self, prefix: str) -> str:
        self.order_seq += 1
        return f"{prefix}-{self.order_seq}-{uuid.uuid4().hex[:6]}"

    def place_market(self, side: str, size: float) -> str:
        oid = self.new_id("mkt")
        self.send(
            {
                "type": "place",
                "id": oid,
                "order_type": "MARKET",
                "side": side,
                "size": size,
            }
        )
        return oid

    def place_limit(self, side: str, price: float, size: float) -> str:
        oid = self.new_id("lmt")
        self.send(
            {
                "type": "place",
                "id": oid,
                "order_type": "LIMIT",
                "side": side,
                "price": price,
                "size": size,
            }
        )
        return oid

    def place_stop(self, side: str, stop: float, size: float) -> str:
        oid = self.new_id("stp")
        self.send(
            {
                "type": "place",
                "id": oid,
                "order_type": "STOP",
                "side": side,
                "stop": stop,
                "size": size,
            }
        )
        return oid

    def on_message(self, msg: dict[str, Any], args: argparse.Namespace) -> None:
        typ = msg.get("type")
        if typ == "book":
            self.last_book = msg
            self.cash = float(msg.get("trader_cash", self.cash))
            self.inventory = float(msg.get("trader_inventory", self.inventory))
            self.maybe_trade(msg, args)
        elif typ == "fill":
            side = msg.get("side")
            px = float(msg["price"])
            sz = float(msg["size"])
            print(f"FILL {side} {sz} @ {px:.4f}", flush=True)
            if side == "BUY" and args.stop_ticks > 0:
                stop = px - args.stop_ticks * args.tick
                sid = self.place_stop("SELL", stop, sz)
                self.open_stops[sid] = "SELL"
                print(f"armed SELL stop @ {stop:.4f}", flush=True)
            elif side == "SELL" and args.stop_ticks > 0:
                stop = px + args.stop_ticks * args.tick
                sid = self.place_stop("BUY", stop, sz)
                self.open_stops[sid] = "BUY"
                print(f"armed BUY stop @ {stop:.4f}", flush=True)
        elif typ == "reject":
            print(f"REJECT {msg}", flush=True)
        elif typ == "hello":
            print(f"server: {msg.get('msg')}", flush=True)

    def maybe_trade(self, book: dict[str, Any], args: argparse.Namespace) -> None:
        now = time.time()
        if now - self.last_action_t < self.cooldown_s:
            return

        mid = float(book["mid"])
        bid = float(book["bid"])
        ask = float(book["ask"])
        spread = float(book.get("spread", ask - bid))
        bids = book.get("bids") or []
        asks = book.get("asks") or []

        if not bids or not asks:
            return

        if ask < mid - args.edge:
            self.place_market("BUY", args.size)
            self.last_action_t = now
            print(f"MARKET BUY ask={ask:.4f} mid={mid:.4f}", flush=True)
            return
        if bid > mid + args.edge:
            self.place_market("SELL", args.size)
            self.last_action_t = now
            print(f"MARKET SELL bid={bid:.4f} mid={mid:.4f}", flush=True)
            return

        if spread >= args.min_spread:
            buy_px = bid + args.tick
            sell_px = ask - args.tick
            acted = False
            if buy_px < mid:
                self.place_limit("BUY", buy_px, args.size)
                print(f"LIMIT BUY @ {buy_px:.4f} (spread={spread:.4f})", flush=True)
                acted = True
            if sell_px > mid:
                self.place_limit("SELL", sell_px, args.size)
                print(f"LIMIT SELL @ {sell_px:.4f} (spread={spread:.4f})", flush=True)
                acted = True
            if acted:
                self.last_action_t = now

    def run(self, args: argparse.Namespace) -> None:
        self.connect()
        last_status = 0.0
        try:
            while True:
                for line in self._recv_lines():
                    try:
                        msg = json.loads(line)
                    except json.JSONDecodeError:
                        continue
                    self.on_message(msg, args)
                now = time.time()
                if now - last_status > 2.0 and self.last_book is not None:
                    pnl = float(self.last_book.get("trader_pnl", 0.0))
                    print(
                        f"status cash={self.cash:.2f} inv={self.inventory:.2f} pnl={pnl:.2f} "
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
    p.add_argument("--edge", type=float, default=0.15, help="mid-edge for market hits")
    p.add_argument("--min-spread", type=float, default=0.8, help="post limits if spread >= this")
    p.add_argument("--tick", type=float, default=0.05)
    p.add_argument("--stop-ticks", type=float, default=4.0, help="0 disables protective stops")
    args = p.parse_args()

    client = TraderClient(args.host, args.port)
    try:
        client.run(args)
    except OSError as exc:
        print(f"connect failed: {exc}", file=sys.stderr)
        print("Start market_maker and click Enable Trader Port first.", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
