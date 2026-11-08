#!/usr/bin/env python3
"""UDP proxy that degrades the network between r-type clients and the server.

Clients connect to the proxy instead of the server; every datagram, in both directions, can be dropped,
delayed (with jitter, which also reorders packets) and duplicated. It needs no privileges and runs on Linux and
Windows, unlike tc/netem.

    ./r-type_server --stats
    python3 scripts/netsim.py --listen 5000 --server 127.0.0.1:4242 --loss 10 --delay 80 --jitter 30 --dup 2
    ./r-type_client 127.0.0.1 5000          # press F3 in game to see ping, bandwidth and loss

Every client gets its own socket towards the server, so the server sees them as separate players.
"""

import argparse
import asyncio
import random
import sys


class Link:
    """What the network does to the datagrams of one direction."""

    def __init__(self, loss, delay_ms, jitter_ms, duplicate, rng):
        self.loss = loss
        self.delay_ms = delay_ms
        self.jitter_ms = jitter_ms
        self.duplicate = duplicate
        self.rng = rng
        self.forwarded = 0
        self.dropped = 0
        self.duplicated = 0
        self.bytes = 0

    def deliver(self, loop, data, send):
        """Schedules send(data) zero, one or two times, after a random delay."""
        if self.rng.random() * 100.0 < self.loss:
            self.dropped += 1
            return
        copies = 1
        if self.rng.random() * 100.0 < self.duplicate:
            copies = 2
            self.duplicated += 1
        for _ in range(copies):
            delay = max(0.0, self.delay_ms + self.rng.uniform(-self.jitter_ms, self.jitter_ms))
            self.forwarded += 1
            self.bytes += len(data)
            loop.call_later(delay / 1000.0, send, data)


class Backend(asyncio.DatagramProtocol):
    """Socket towards the server, dedicated to one client."""

    def __init__(self, proxy, client_addr):
        self.proxy = proxy
        self.client_addr = client_addr
        self.transport = None

    def connection_made(self, transport):
        self.transport = transport

    def datagram_received(self, data, addr):
        self.proxy.down.deliver(
            self.proxy.loop, data, lambda payload: self.proxy.front.sendto(payload, self.client_addr))

    def error_received(self, exc):
        pass


class Proxy(asyncio.DatagramProtocol):
    def __init__(self, loop, server_addr, up, down):
        self.loop = loop
        self.server_addr = server_addr
        self.up = up
        self.down = down
        self.front = None
        self.backends = {}

    def connection_made(self, transport):
        self.front = transport

    def datagram_received(self, data, addr):
        backend = self.backends.get(addr)
        if backend is None:
            backend = Backend(self, addr)
            self.backends[addr] = backend
            asyncio.ensure_future(self.loop.create_datagram_endpoint(
                lambda: backend, remote_addr=self.server_addr))
            print(f"[netsim] new client {addr[0]}:{addr[1]}", flush=True)
        self.up.deliver(self.loop, data, lambda payload: self.send_up(backend, payload))

    @staticmethod
    def send_up(backend, payload):
        if backend.transport is not None:
            backend.transport.sendto(payload)

    def error_received(self, exc):
        pass


def parse_address(text):
    host, _, port = text.rpartition(":")
    return host or "127.0.0.1", int(port)


async def report(up, down, every):
    while True:
        await asyncio.sleep(every)
        print(f"[netsim] up: {up.forwarded} sent, {up.dropped} dropped, {up.duplicated} duplicated | "
              f"down: {down.forwarded} sent, {down.dropped} dropped, {down.duplicated} duplicated "
              f"({down.bytes / 1024:.0f} KB)", flush=True)


async def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--listen", type=int, default=5000, help="port the clients connect to (default 5000)")
    parser.add_argument("--server", default="127.0.0.1:4242", help="real server, host:port (default 127.0.0.1:4242)")
    parser.add_argument("--loss", type=float, default=0.0, help="percent of datagrams dropped, each way")
    parser.add_argument("--delay", type=float, default=0.0, help="latency added to each datagram, in ms")
    parser.add_argument("--jitter", type=float, default=0.0, help="random +/- ms on the delay (reorders packets)")
    parser.add_argument("--dup", type=float, default=0.0, help="percent of datagrams delivered twice")
    parser.add_argument("--seed", type=int, default=None, help="seed of the random generator, for repeatable runs")
    parser.add_argument("--report", type=float, default=5.0, help="seconds between two reports (0 to disable)")
    args = parser.parse_args()
    for name in ("loss", "dup"):
        if not 0.0 <= getattr(args, name) <= 100.0:
            parser.error(f"--{name} must be between 0 and 100")
    for name in ("delay", "jitter"):
        if getattr(args, name) < 0.0:
            parser.error(f"--{name} cannot be negative")

    loop = asyncio.get_running_loop()
    rng = random.Random(args.seed)
    up = Link(args.loss, args.delay, args.jitter, args.dup, rng)
    down = Link(args.loss, args.delay, args.jitter, args.dup, rng)
    proxy = Proxy(loop, parse_address(args.server), up, down)
    await loop.create_datagram_endpoint(lambda: proxy, local_addr=("0.0.0.0", args.listen))
    print(f"[netsim] port {args.listen} -> {args.server} | loss {args.loss}% delay {args.delay}ms "
          f"jitter {args.jitter}ms duplicate {args.dup}% (each way)", flush=True)
    if args.report > 0:
        asyncio.ensure_future(report(up, down, args.report))
    await asyncio.Event().wait()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        sys.exit(0)
