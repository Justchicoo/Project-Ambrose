<!-- Project Ambrose by Imjustchico: C-45 guidance for exposing an Ambrose server safely on a network. -->

# C-45: Opening a server to the internet safely

Project Ambrose is pre-alpha. A public deployment should expose as little as possible and should not treat the current admin listener as internet-safe: the configuration reference says TLS is not served by the admin API yet, and remote plain HTTP is an explicit insecure opt-in.

## The safest default

Keep every server and admin listener on loopback or a private network unless a specific deployment requirement has been reviewed:

```ini
BindIP = 127.0.0.1
Admin.BindIP = 127.0.0.1
```

Use the documented `.conf.dist` as the starting point. Do not copy a local `.conf` containing passwords, tokens, database connection strings, or private paths into the repository.

The game, login, patch, and admin ports are separate settings. Open only the port required by the client or by a trusted operator network. A process listening on `0.0.0.0` is reachable through every interface unless the host firewall narrows it.

## Do not expose the current admin API directly

The admin token is sent as an `Authorization` bearer token. `Admin.Token` may read it from an environment variable, but the documented preference is `Admin.TokenFile`, whose generated file is readable only by the account running the server. Never put a real token in a committed configuration, issue description, shell transcript, screenshot, or public URL.

The current admin configuration has these important limits:

- `Admin.BindIP` defaults to loopback.
- A non-loopback admin bind is refused unless the operator explicitly enables `Admin.AllowPlainHttpRemote` or supplies the certificate settings accepted by the current version.
- TLS is not served yet; a remote plain-HTTP binding sends the token, commands, and log lines unencrypted.
- `Admin.AuthFailureBurst` and `Admin.AuthFailuresPerSecond` limit failed authentication by caller address.
- `Admin.MaxRequestBytes` bounds the request body and WebSocket message delivered to the handler, but the reference notes that the HTTP layer buffers a body before that check.

Because of these facts, do not forward the admin port through a router, reverse proxy, tunnel, or port-forwarding service as a substitute for TLS. Keep it loopback-only and administer the host through a protected management channel until the project documents a supported TLS boundary.

## Host firewall rules

Use the host firewall as a second boundary, not as a replacement for correct bind addresses. The exact commands depend on the operating system and must be reviewed before applying them. The rule should:

1. deny unsolicited inbound traffic by default;
2. allow the login or game port only from the clients or private network that need it;
3. allow the patch port only if patch serving is actually enabled;
4. deny the admin port from all remote addresses;
5. allow established and related return traffic; and
6. log denied attempts without logging credentials or full request headers.

Do not use a broad “allow the executable” rule when a port- and profile-scoped rule is possible. Recheck the effective firewall rules after a server restart and after changing a bind or port setting.

## If a public game service is unavoidable

Treat the public listener as an experiment with a narrow rollback:

- use a dedicated host or isolated VM;
- run the service under a non-administrator account;
- keep the database listener private and allow database access only from the server host;
- use separate, least-privilege database credentials for each application;
- disable unused applications and ports;
- keep `Admin.BindIP` on loopback;
- keep backups and logs outside public web roots;
- patch the operating system and dependencies before opening the port;
- monitor connection, authentication, and listener errors; and
- have a firewall rule or host shutdown ready to close the service.

Do not promise that a public server is secure merely because it starts successfully or because `--check` passes. Startup validation proves configuration and readiness, not host hardening, authorization policy, or resistance to denial of service.

## Reverse proxies and TLS

The repository has a separate contributor item for a reverse-proxy and TLS guide. Do not invent certificate handling for the current admin API. When a supported TLS boundary exists, it must document certificate storage, private-key permissions, forwarded-address handling, token redaction, connection limits, and how the proxy prevents direct access to the backend listener.

Until then, a proxy that merely forwards plain HTTP is not encryption. A VPN or private management network can reduce exposure, but it does not turn an unauthenticated or misconfigured admin listener into a public service.

## Verify before opening a port

Use a disposable configuration and check:

1. which process owns each listening port;
2. which local interfaces the process binds;
3. the effective host firewall rule;
4. that the admin port is unreachable from a second machine or namespace;
5. that the database port is unreachable from the public interface;
6. that logs contain no token, password, or full connection string; and
7. that a configuration reload does not unexpectedly widen a binding.

Run the smallest relevant application checks after configuration changes. A failed bind, invalid remote-admin setting, missing token, or unsafe certificate combination should be fixed from its named error rather than bypassed by enabling a broader listener.

## Emergency shutdown

If an admin token, password, session key, or private key may have crossed a public or unencrypted connection:

1. close the exposed listener at the firewall;
2. stop the affected service if access cannot be ruled out;
3. rotate the token and every credential that was sent or stored with it;
4. inspect logs for unauthorized requests without publishing them;
5. restore the narrow bind and firewall rules; and
6. preserve only redacted evidence for a private report.

Deleting a log does not revoke a credential that may already have been observed.

## Cheapest disproof

Before opening any port, run the service with `BindIP` and `Admin.BindIP` set to loopback and inspect the listening sockets from the host and a second private machine. If the admin port is reachable remotely, the proposed boundary is already false and must be fixed before any public test. If a firewall rule says the port is closed but the process listens on every interface, the firewall is compensating for a configuration error rather than solving it.

## Verification and limitations

This guide was checked against the current login-server, gameserver, and patchserver configuration references and the admin settings tests on 2026-09-19. It does not claim that any particular operating-system firewall command is universally correct, and it does not claim that the current admin API supports public TLS. Confirm the exact configuration reference and platform firewall behavior before exposing a service.
