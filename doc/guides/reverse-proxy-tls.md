<!-- Project Ambrose by Imjustchico: C-44 guidance for putting a loopback-bound Ambrose admin API behind a TLS reverse proxy. -->

# C-44: Running Ambrose behind a reverse proxy with TLS

The current Ambrose admin API does not serve TLS itself. The safe boundary today is a local reverse proxy that owns the public HTTPS socket and forwards to an admin listener bound to loopback. Do not bind the Ambrose admin API to a public address just because a proxy exists.

## Target topology

Use this shape:

```text
operator browser
        |
        | HTTPS, public or private operator address
        v
reverse proxy :443
        |
        | HTTP on loopback only
        v
Ambrose Admin.BindIP=127.0.0.1
```

The proxy and the Ambrose process should run on the same host unless the project documents a mutually authenticated private transport. A proxy on another host cannot safely forward the admin token over an untrusted plain-HTTP network.

Keep the game, login, and patch listeners separate from the admin listener. A TLS proxy for the panel does not encrypt or authenticate those other protocols.

## Configure Ambrose for the backend

Start from the application’s shipped `.conf.dist` and set only the local admin values needed by the deployment:

```ini
Admin.BindIP = 127.0.0.1
Admin.Port = 12010
Admin.TokenFile = <private path outside the repository>
Admin.AllowPlainHttpRemote = 0
```

The exact default admin port differs by application; use that application’s configuration reference rather than copying the example blindly. Set `Admin.Port` to a local free port and confirm the process owns it after startup.

Prefer `Admin.TokenFile` or the documented environment variable over `Admin.Token`. The token file must be readable only by the account running Ambrose and must never be placed in a web root, repository, public backup, or proxy configuration committed to source control.

Do not set `Admin.BindIP` to `0.0.0.0` or a public interface. Do not enable `Admin.AllowPlainHttpRemote` for this topology. That option allows a remote plain-HTTP binding, which would expose the bearer token and commands to anyone on the path.

## Configure the proxy

Use the reverse proxy’s own documented TLS configuration and write a minimal route that:

- listens on the intended HTTPS address;
- redirects or refuses plain HTTP;
- forwards only the required admin path;
- connects upstream to `http://127.0.0.1:<Admin.Port>`;
- preserves the request method and body;
- sets a bounded request and response timeout;
- limits request size to the application’s documented maximum;
- does not log the `Authorization` header or query-string secrets;
- does not expose a directory or a second backend; and
- sends a stable public health response only if the operator intentionally wants one.

The project has not settled a universal proxy product, configuration syntax, or forwarded-header trust policy. Use the proxy’s current documentation for certificate loading, renewal, private-key permissions, header filtering, and WebSocket support if an admin endpoint needs it. Do not assume that `X-Forwarded-For`, `Forwarded`, or a proxy-added identity header is trusted by Ambrose unless the application documentation explicitly says so.

If the dashboard is served separately from the API, keep its origin and API route explicit. Do not use a wildcard CORS policy or forward arbitrary paths to the admin listener.

## Certificates and keys

Use a certificate whose names cover the operator-facing hostname. Store the private key outside the repository with permissions limited to the proxy account. Renew it before expiry and test renewal without widening the backend bind.

The proxy must reject:

- expired or malformed certificates;
- requests with an invalid Host name when the proxy supports host filtering;
- plain HTTP after the redirect or refusal policy;
- unsupported methods and oversized bodies; and
- direct access to the backend port from remote interfaces.

Do not put the Ambrose bearer token in the certificate, URL, access log, command line, or certificate-renewal output. TLS protects the connection; it does not replace the admin token.

## Firewall and host boundaries

The host firewall should allow the proxy’s HTTPS port only from the intended operator network. It should deny the Ambrose admin port from every remote address and allow the game-facing ports separately according to the deployment’s actual needs.

After starting both processes, inspect listening sockets from the host and from another machine. The public machine may show the proxy’s port; it must not show the Ambrose admin port on a non-loopback address.

If the proxy runs under a separate account, ensure that it can connect to the loopback port without granting it access to the token file. The proxy should forward the browser’s `Authorization` header only when the operator intentionally uses the Ambrose token; never copy the token into a proxy-wide static header.

## Verify the complete path

Use a temporary hostname or private DNS name and a disposable token while testing:

1. Start Ambrose with `Admin.BindIP=127.0.0.1`.
2. Confirm the backend answers on loopback and that the public interface cannot reach `Admin.Port`.
3. Start the proxy with the test certificate.
4. Confirm HTTPS negotiation and certificate-name validation from the operator client.
5. Confirm plain HTTP is redirected or refused as configured.
6. Confirm a request without the token is rejected by the admin API.
7. Confirm a wrong token is rejected and the proxy does not log it.
8. Confirm a valid token reaches one existing read-only endpoint.
9. Confirm an oversized request is rejected without a handler seeing an unbounded body.
10. Confirm proxy access logs contain no token, password, session key, or full database connection string.
11. Stop the proxy and verify the loopback backend is not reachable from the operator network.
12. Rotate the disposable token and remove the temporary certificate and logs.

Run the smallest relevant application checks after changing the Ambrose configuration. A successful proxy request does not prove that the game, login, patch, database, or client paths are healthy.

## Failure and rollback

If the proxy cannot load its certificate, stop it rather than falling back to a plain public listener. If the backend refuses its configuration, fix the named setting rather than enabling remote plain HTTP. If a token, password, or private key appears in logs, command history, or a captured request, close the listener and rotate the credential before investigating further.

Rollback is:

1. remove the public DNS or firewall allowance;
2. stop the proxy;
3. restore the admin bind to loopback;
4. rotate the admin token if exposure is possible; and
5. retain only redacted evidence.

## Cheapest disproof

Before calling the deployment safe, connect from a second machine to the Ambrose admin port and to the proxy’s HTTPS port. The first must fail from the remote network, while the second must complete TLS and then require the admin token. If both ports are reachable, or if the backend accepts a request without the token, the topology is not the one this guide describes.

## Verification and limitations

This guide was checked against the current application configuration references, admin settings tests, and project architecture on 2026-09-19. It does not claim that Ambrose currently terminates TLS itself, that every admin route supports WebSockets, or that forwarded client-address headers are trusted. Confirm those details in the application version and proxy documentation before deployment.
