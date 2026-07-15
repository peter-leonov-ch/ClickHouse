# Deploying `clickhouse-wsproxy` on Cloudflare (Containers + Workers)

Feasibility notes and a deployment sketch for running the proxy as a Cloudflare Container fronted
by stateless Workers. **Status: analysis only — nothing here is built yet.** See `TODO.md` (repo
root) for the proxy's overall status, and `programs/wsproxy/bench/` for the throughput/compression
numbers this design leans on. For the alternative of putting a WebSocket port *inside*
`clickhouse-server` (rather than as this edge sidecar), see `IN_SERVER.md`.

## Verdict

**Moderate effort, strong fit.** The proxy is already container-shaped: a single self-contained
binary, configured entirely by `WSPROXY_*` env vars, holding **no local/disk state** (all session
state is in-memory per connection), speaking WebSocket in and the native protocol out. Running it in
a container needs essentially no code change; the work is the amd64 Linux build plus a thin
Worker/Durable Object glue layer.

It is arguably a *better* showcase of the value prop than a k8s sidecar, because the Worker gives you
a programmable, DDoS-protected, auth-capable front door for free, and the compressed-native-to-region
hop is exactly what the pitch is about.

## Architecture: stateless Worker + stateful container

The proxy's stateless/stateful split maps cleanly onto the platform:

- **Worker = stateless front door / router.** Terminates the client WSS at the true edge, does
  auth/routing, and forwards the upgrade to a container. Holds no session state.
- **Container = the stateful proxy.** Runs the real binary, owns the per-session native `Connection`
  and in-flight query, and does the format conversion. This *must* be a Container, not a Worker: the
  value is the C++ `FormatFactory` + native protocol, which cannot realistically be reimplemented in
  a Worker (JS/WASM) — that is the "clickhouse-cpp has no formats" problem again.
- **Compressed hop to the region.** The container opens the outbound native + **ZSTD** connection to
  ClickHouse Cloud (`:9440`). Converting at the container and keeping the ClickHouse-region leg
  compressed is the whole point (see the compression findings in `bench/README.md`).

```
client  --WSS-->  Worker (true edge PoP)  --CF backbone-->  Container (regional)  --native+zstd (WAN)-->  ClickHouse Cloud
                  stateless router                          stateful proxy = a Durable Object
```

### A Container *is* a Durable Object (verified)

The `Container` class from the [`@cloudflare/containers`](https://github.com/cloudflare/containers)
library **extends `DurableObject`**. Cloudflare's docs state it directly: *"a request passes through
a Durable Object instance (the Container class extends a Durable Object class)."* Each container
instance is backed by **exactly one** DO instance — the DO is the programmable sidecar that owns the
container's lifecycle and routing; the container is the workload attached to it.

Consequences:

- Your container subclass gets a free per-instance place for coordination logic — warm-up, health,
  draining, `alarm`-based idle handling, small storage — with no extra moving part.
- The DO is single-threaded JS, but it is **not** in the per-frame data path: the client WebSocket
  rides the container's exposed port via the stub's `fetch`; the DO only manages lifecycle. So
  "DO is single-threaded" is **not** a throughput bottleneck for the proxy — as long as you forward
  to the container port and do not proxy bytes through DO JS.
- **A DO and its container may run in different locations** (placement is optimized for routing and
  startup). This feeds the placement nuance below.

## Running many proxy containers (scaling out)

The proxy's sessions are **interchangeable** — each is just a fresh native `Connection`, a stateless
pool — which is exactly the case the platform's helpers target:

- `getRandom(env.WSPROXY, N)` — picks a random instance out of `N`. The stateless load balancer, and
  the right fit here: each client WS upgrade → random container → new backend connection. The
  WebSocket then naturally pins to that instance for its lifetime.
- `getContainer(env.WSPROXY, name)` — addresses a *specific* named instance by ID (sticky/stateful
  routing). Probably unneeded for the proxy, but available.
- Multiple instances = multiple DO IDs; the library manages spin-up, pulling pre-fetched images at
  other locations for fast cold starts.

The Worker glue is small:

```js
import { getRandom } from "@cloudflare/containers";

export default {
  async fetch(request, env) {
    // Optional: authenticate here, or let creds pass through to the container -> ClickHouse.
    const instance = getRandom(env.WSPROXY, N); // N interchangeable proxy containers
    return instance.fetch(request);             // forwards the WS upgrade to the container port
  },
};
```

**The catch — no autoscaling yet.** You pick and manage `N` yourself (a fixed fleet, or your own
logic). Cloudflare says built-in autoscaling is planned but not available today. Capacity ≈
`N × sessions-per-container`, so size `N` against a per-container session cap (see the thread-per-
session note below). Until autoscaling lands, elasticity is on you — over-provision a fixed `N`, or
build a small coordinator (itself a DO) that adjusts routing under load.

## Concrete work items

1. **x86_64 Linux build** *(the long pole).* Everything so far was built on macOS/arm64; Cloudflare
   Containers are **amd64 Linux only**. ClickHouse builds amd64 Linux routinely (CI does), so this is
   well-trodden — just not yet done for this binary.
2. **Dockerfile.** A slim/distroless base + the ~306 MB binary + env; expose the listen port
   (`WSPROXY_PORT`), set a `CMD`. Image ≈ 400-500 MB (under the platform's GB-scale image limits —
   verify current limits).
3. **Worker + Durable Object + `wrangler` config.** ~50-150 lines: accept the WS upgrade, `getRandom`
   to a container, forward. Secrets (backend password) via Worker/container secrets → container env.
4. **Secrets & egress.** Map `WSPROXY_BACKEND_*` into the container; settle ClickHouse-side source-IP
   policy (see egress gotcha).

Rough estimate: **PoC in 1-2 days** (the Linux build dominates), **production-ready in 1-2 weeks**
once the frictions below are handled.

## Frictions / gotchas (Cloudflare-specific)

- **Thread-per-session vs small containers *(biggest one)*.** The proxy is thread-per-session blocking
  IO with default ~8 MB stacks → ~100 sessions ≈ ~800 MB just in stacks, and container instances are
  memory-capped. Sessions-per-container is therefore bounded; lean on `getRandom` across instances
  and/or shrink the thread stack size (a small proxy change worth doing before serious fan-in).
- **CPU limits cap the conversion win.** Formatting is CPU-heavy and `?parallel` wants multiple cores,
  but instance types meter vCPU. Right-size the instance; the parallel-formatting speedup is bounded
  by the vCPU allotted.
- **Egress IP allowlisting.** The container's outbound to ClickHouse Cloud uses Cloudflare's
  shared/dynamic egress IPs. If the service restricts source IPs, you need CH-side allow-all or
  Cloudflare egress ranges — settle this early (security posture).
- **Cold starts.** Containers sleep on idle; the first connection pays container boot + proxy start +
  backend handshake (seconds). Eager-connect adds to that.
- **Placement affects the compression benefit.** Containers run in *regional* locations, not literally
  every edge PoP, and may not co-locate with their DO. A container near the ClickHouse region → short
  compressed hop (smaller win); near the user → long compressed hop (bigger win). Less placement
  control than k8s.
- **Keep the DO out of the per-frame data path** (forward to the container port; do not proxy frames
  through DO JS).

## Model shift: 1:1 sidecar → shared multi-tenant

On k8s the proxy is a strict 1:1 sidecar (one proxy per app). On Cloudflare you would run a **shared
multi-tenant** pool (many sessions per container, the Worker fanning out across instances). The proxy
already supports this safely: it does **per-session credential pass-through** and keeps zero
cross-session state, so multi-tenant is fine as long as each session carries its own credentials.

## Alternative considered — pure Worker (no container)

Workers can open outbound TCP (`connect` from `cloudflare:sockets`), so in principle the proxy could
live entirely in a Worker. Rejected: it would require reimplementing the ClickHouse native protocol
**and** the full format matrix in JS/WASM. Compiling ClickHouse itself to WASM is not feasible. The
container is the right vehicle precisely because it runs the real C++ binary with full format
coverage.

## Sources

- [Lifecycle of a Container — architecture](https://developers.cloudflare.com/containers/platform-details/architecture/)
- [Scaling and Routing](https://developers.cloudflare.com/containers/platform-details/scaling-and-routing/)
- [`cloudflare/containers` library README](https://github.com/cloudflare/containers/blob/main/README.md)
- [Containers overview](https://developers.cloudflare.com/containers/)
- [Containers coming to Workers (announcement)](https://blog.cloudflare.com/cloudflare-containers-coming-2025/)

*(Cloudflare Containers are a recently-GA product; exact limits and APIs move — verify image-size,
memory, vCPU, WS-to-container specifics, and egress ranges against the live docs before committing.)*
