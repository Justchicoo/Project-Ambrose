<!-- Project Ambrose by Imjustchico: Operator runbook for the console, admin API, metrics stack, safe remote access, and restart boundaries. -->

# Operations

## Console and dashboard

Run each server headlessly under the supervisor and use its console for local
commands such as `reload` and `shutdown`. The supervisor dashboard and admin
API are the operator-facing control plane. Keep the admin listener on loopback
until remote access is protected by TLS and an explicit trusted-proxy policy.

The Prometheus endpoint is `GET /metrics`. It is guarded by the same admin
token permission as the status API. Give Prometheus a bearer token through a
local file; never put the token in this repository, a Compose file, a
dashboard URL, or a log excerpt.

## Local metrics stack

From `apps/grafana`, create an untracked `.env`:

```dotenv
GRAFANA_ADMIN_PASSWORD=choose-a-local-password
```

Prometheus reads its target literally from
`apps/grafana/prometheus/prometheus.yml`; change `host.docker.internal:12343`
there before starting the stack when the gameserver listens somewhere else.
Prometheus does not expand environment variables in that file.

Put the token accepted by the gameserver in
`apps/grafana/secrets/metrics_token` (this path is ignored and must remain
outside Git). Start the stack next to a running gameserver:

```powershell
docker compose up -d
docker compose ps
```

Open Grafana at `http://127.0.0.1:3000`, sign in with the configured admin
credentials, and select the three provisioned dashboards in the **Ambrose**
folder. Prometheus scrapes every 15 seconds, so a working target appears
within 30 seconds. The Prometheus UI at `http://127.0.0.1:9090/targets`
shows authentication and network errors without exposing the token.

For a gameserver listening on another host, use a private routed address in
`prometheus/prometheus.yml`; do not publish Grafana or Prometheus to all
interfaces. The stack is deliberately not a public monitoring service.

## Safe remote access

Put a TLS reverse proxy such as Caddy or nginx in front of the admin API and
Grafana. Terminate TLS there, restrict inbound addresses with a firewall or
VPN, forward only the original host and client address through the configured
trusted-proxy list, and keep the upstream listeners private. Use a separate
operator identity for each person, rotate tokens after staff changes, and
verify that the proxy rejects plain HTTP and unknown hosts. Never bypass the
admin permission checks by exposing `/metrics` directly.

## Restart-required changes

| Change | Why a restart is required |
| --- | --- |
| Binary upgrade | The running process cannot replace its executable and code safely. |
| Adding or removing a compiled module | Module code and registration are fixed when the process starts. |
| A schema update the new binary needs | The old binary may not understand the new schema or statements. |
| A client revision or type dump change when live objects cannot hold the old registry | Existing objects retain the old registry and cannot safely cross the incompatible boundary. |
| Client-side WAD changes | The client must be re-patched before it can use the changed archive. |

Configuration, logging, message definitions, and data-backed world content
should use their live reload path when one exists. If a reload reports an
error, keep serving the previous generation and fix the named input before
trying again.

## Shutdown and recovery

Use the supervisor's normal shutdown command before `docker compose down`.
Treat forced removal as recovery: inspect logs, confirm database recovery on
the next start, and verify that dashboards resume scraping. Do not store
client files, captures, generated manifests, credentials, or private host
paths in the checkout.
