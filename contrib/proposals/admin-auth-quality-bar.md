<!-- Project Ambrose by Imjustchico: Proposal for an end-to-end quality bar around admin authentication and failed-request throttling. -->

# Proposal: admin authentication quality bar

## Item

C-54: add a quality-bar guard for the server admin HTTP surface.

## What is already covered

The repository already has unit tests for the reusable `TokenBucket`, and the admin authentication code owns a bucket for failed requests. Those tests prove the limiter's arithmetic in isolation. They do not prove that every admin endpoint applies authentication before dispatching a command, that a failed request consumes the intended budget, or that a successful request still works after the budget refills.

This proposal is intentionally an integration guard rather than another `TokenBucket` unit test.

## Proposed guard

Extend the existing application smoke tests with a disposable loopback admin listener. Use a generated token and a temporary configuration, then issue requests with a small deterministic budget supplied through test-only configuration overrides.

The test should make these requests in order:

1. A request without an authorization token.
2. A request with a wrong token.
3. A request to an unknown endpoint with the correct token.
4. A request to a real read-only endpoint with the correct token.
5. Enough additional wrong-token requests to exhaust the configured failed-auth budget.
6. The same real read-only request with the correct token while the budget is exhausted.
7. After advancing the test clock or waiting for one refill interval, the real read-only request with the correct token again.

The exact endpoint should be selected from the admin route that exists in the application under test. The guard must not invent a route or depend on a future panel feature.

## Required assertions

The check should fail when:

- an unauthenticated request reaches an admin handler;
- a wrong token is accepted;
- an unknown route is handled as a successful command;
- a valid token cannot reach an existing read-only endpoint;
- failed authentication does not consume the configured budget;
- a request after budget exhaustion is processed instead of rejected or throttled;
- the budget never refills; or
- the response or logs print the configured token, password, or full database connection string.

The report should include the request number, HTTP status, and a redacted reason for the first failure. It must not include the bearer token or the full configuration.

## Why this boundary is useful

Authentication and rate limiting are security controls at the HTTP boundary. Unit tests for a bucket cannot detect a route that forgets to call the authentication helper, a handler that runs before the limiter, or a response path that accidentally logs credentials. A small loopback test exercises the real listener, parser, authentication layer, limiter, dispatch table, and response redaction without requiring a client or an external service.

The test should use a disposable configuration and loopback only. It must not contact a public address, require TLS certificates, or run KingsIsle software.

## Cheapest disproof

Before implementing this guard, inspect the current admin routes and tests. If an existing application-level test already sends unauthenticated, wrong-token, exhausted-budget, and post-refill requests through the real listener while checking redaction, this proposal is redundant and should be closed.

If the route exists but the admin listener is not yet present in the built application, the proposal must remain documentation only until a contributor-track item or roadmap milestone establishes the smallest stable endpoint. A test that targets a planned route would not prove anything about the current server.

## Dependencies and cost

The implementation needs one built server application with its admin listener, a loopback HTTP client available to the test environment, and a deterministic way to configure or inject the authentication budget and clock. It should reuse the existing app-smoke work directory and timeout conventions rather than create a second test harness.

No client installation, packet capture, database service, or KingsIsle executable is required. If the current test framework cannot control the clock, the refill assertion may use a short bounded wait, provided the test documents the interval and remains reliable on the supported CI platforms.

## Acceptance

The proposal is ready for implementation when a maintainer can point to one test command that:

- starts a real admin listener on loopback with a disposable token;
- proves unauthenticated and wrong-token requests never reach a handler;
- proves the failed-auth budget is enforced and later refills;
- proves an existing valid read-only endpoint still works;
- redacts the token from output and logs;
- leaves no process, listener, token, or temporary configuration behind; and
- runs in the same test preset as the application tests.
