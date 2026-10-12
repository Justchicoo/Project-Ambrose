<!-- Project Ambrose by Imjustchico: The behind-a-proxy page: picks the proxy product and public hostname, renders a ready config that forwards only to the panel listener, runs the forwarded-header connection check with every mismatch explained and its fix named, and sets Panel.TrustedProxies through the audited settings path once the operator has reviewed the config. -->
<script lang="ts">
    import * as Card from "$lib/components/ui/card/index.js";
    import { Button } from "$lib/components/ui/button/index.js";
    import { Input } from "$lib/components/ui/input/index.js";
    import { Label } from "$lib/components/ui/label/index.js";
    import { ApiError } from "$lib/api.svelte.js";
    import { proxyCheck, renderProxyConfig, panelSettings, updatePanelSettings } from "$lib/supervision.svelte.js";
    import type { InferOutput } from "valibot";
    import { ProxyCheckAnswer, ProxyConfigAnswer } from "$lib/schemas.js";
    import PageHeader from "../components/PageHeader.svelte";

    type Check = InferOutput<typeof ProxyCheckAnswer>;
    type Rendered = InferOutput<typeof ProxyConfigAnswer>;

    const products = [
        { id: "nginx", label: "nginx" },
        { id: "caddy", label: "Caddy" },
        { id: "apache", label: "Apache httpd" },
        { id: "haproxy", label: "HAProxy" },
        { id: "traefik", label: "Traefik" },
        { id: "iis", label: "IIS with ARR" },
    ];

    let product = $state("nginx");
    let hostname = $state("");
    let includePatchserver = $state(false);
    let rendered = $state<Rendered | null>(null);
    let check = $state<Check | null>(null);
    let trustedLocked = $state(false);
    let failure = $state("");
    let notice = $state("");
    let busy = $state(false);
    let checking = $state(false);
    let socketProbe = $state<"idle" | "ok" | "failed" | "running">("idle");

    async function runCheck() {
        checking = true;
        failure = "";
        try {
            check = await proxyCheck();
        } catch (problem) {
            failure = problem instanceof ApiError ? problem.message : "The connection check could not run";
        } finally {
            checking = false;
        }
    }

    async function loadTrusted() {
        const answer = await panelSettings().catch(() => null);
        const setting = answer?.settings.find((entry) => entry.key === "Panel.TrustedProxies");
        trustedLocked = setting?.locked ?? false;
    }

    $effect(() => {
        void runCheck();
        void loadTrusted();
    });

    async function render() {
        busy = true;
        failure = "";
        notice = "";
        rendered = null;
        try {
            rendered = await renderProxyConfig(product, hostname.trim(), includePatchserver);
        } catch (problem) {
            failure = problem instanceof ApiError ? problem.message : "The config could not be rendered";
        } finally {
            busy = false;
        }
    }

    async function trustPeer() {
        if (!check) return;
        busy = true;
        failure = "";
        notice = "";
        try {
            await updatePanelSettings({ "Panel.TrustedProxies": check.peer });
            notice = `Panel.TrustedProxies set to ${check.peer} and audited.`;
            await loadTrusted();
            await runCheck();
        } catch (problem) {
            failure = problem instanceof ApiError ? problem.message : "The trusted proxies could not be saved";
        } finally {
            busy = false;
        }
    }

    async function copyConfig() {
        if (!rendered) return;
        try {
            await navigator.clipboard.writeText(rendered.config);
            notice = "Config copied to the clipboard.";
        } catch {
            failure = "The clipboard is not available; select the text by hand.";
        }
    }

    function probeSocket() {
        socketProbe = "running";
        const url = `${location.protocol === "https:" ? "wss" : "ws"}://${location.host}/api/panel/events`;
        try {
            const socket = new WebSocket(url);
            const timer = window.setTimeout(() => {
                socket.close();
                if (socketProbe === "running") socketProbe = "failed";
            }, 8000);
            socket.addEventListener("open", () => {
                window.clearTimeout(timer);
                socketProbe = "ok";
                socket.close();
            });
            socket.addEventListener("error", () => {
                window.clearTimeout(timer);
                socketProbe = "failed";
            });
        } catch {
            socketProbe = "failed";
        }
    }
</script>

<PageHeader
    title="Reverse proxy"
    description="Run the panel behind Caddy, nginx, Apache, HAProxy, Traefik or IIS, and check that its forwarded headers are trusted."
/>

{#if failure}
    <p class="rounded-md border border-destructive/30 bg-destructive/5 p-3 text-sm text-destructive" role="alert">{failure}</p>
{/if}
{#if notice}
    <p class="rounded-md border border-healthy/30 bg-healthy/5 p-3 text-sm text-healthy" role="status">{notice}</p>
{/if}

<div class="grid gap-4 lg:grid-cols-2">
    <Card.Root class="shadow-xs">
        <Card.Header>
            <Card.Title>Connection check</Card.Title>
            <Card.Description>What the panel actually received on this request.</Card.Description>
        </Card.Header>
        <Card.Content class="space-y-3 text-sm">
            <Button onclick={() => void runCheck()} disabled={checking}>{checking ? "Checking…" : "Run the check"}</Button>
            {#if check}
                <dl class="grid grid-cols-[auto_1fr] gap-x-4 gap-y-1">
                    <dt class="text-muted-foreground">Peer</dt>
                    <dd class="font-mono">{check.peer}</dd>
                    <dt class="text-muted-foreground">Peer trusted</dt>
                    <dd>{check.peer_trusted ? "yes" : "no"}</dd>
                    <dt class="text-muted-foreground">Client address</dt>
                    <dd class="font-mono">{check.client_address}</dd>
                    <dt class="text-muted-foreground">Scheme</dt>
                    <dd class="font-mono">{check.scheme}</dd>
                    <dt class="text-muted-foreground">Host</dt>
                    <dd class="font-mono">{check.host}</dd>
                    <dt class="text-muted-foreground">Public URL</dt>
                    <dd class="font-mono">{check.public_url || "not set"}</dd>
                    <dt class="text-muted-foreground">Trusted list</dt>
                    <dd class="font-mono">
                        {check.trusted_proxies || "empty"}{check.trusted_proxies_locked
                            ? ` (locked by ${check.trusted_proxies_layer})`
                            : ""}
                    </dd>
                </dl>
                {#if Object.keys(check.forwarded_headers).length > 0}
                    <div>
                        <p class="text-muted-foreground">Forwarded headers received</p>
                        <ul class="mt-1 space-y-1">
                            {#each Object.entries(check.forwarded_headers) as [name, value] (name)}
                                <li class="font-mono text-xs">{name}: {String(value)}</li>
                            {/each}
                        </ul>
                    </div>
                {/if}
                {#each check.problems as problem (problem.message)}
                    <div class="rounded-md border border-amber-500/30 bg-amber-500/5 p-3">
                        <p class="font-medium">{problem.message}</p>
                        <p class="mt-1 text-muted-foreground">Fix: {problem.fix}</p>
                    </div>
                {/each}
                {#if check.problems.length === 0}
                    <p class="text-healthy">No mismatches: the panel sees the real client.</p>
                {/if}
                {#if !check.peer_trusted && !trustedLocked}
                    <Button onclick={() => void trustPeer()} disabled={busy}>Trust {check.peer} as the proxy</Button>
                {/if}
                <div class="flex items-center gap-2">
                    <Button variant="outline" onclick={probeSocket} disabled={socketProbe === "running"}>
                        {socketProbe === "running" ? "Probing…" : "Probe the event socket"}
                    </Button>
                    {#if socketProbe === "ok"}<span class="text-healthy text-sm">The socket upgrade got through.</span>{/if}
                    {#if socketProbe === "failed"}<span class="text-destructive text-sm"
                            >The socket did not connect; compare the proxy config with the generated one.</span
                        >{/if}
                </div>
            {/if}
        </Card.Content>
    </Card.Root>

    <Card.Root class="shadow-xs">
        <Card.Header>
            <Card.Title>Proxy config</Card.Title>
            <Card.Description>Review it, install it on the proxy, then trust the proxy above.</Card.Description>
        </Card.Header>
        <Card.Content class="space-y-3 text-sm">
            <div class="grid gap-2">
                <Label for="proxy-product">Proxy product</Label>
                <select id="proxy-product" class="rounded-md border bg-background p-2" bind:value={product}>
                    {#each products as option (option.id)}
                        <option value={option.id}>{option.label}</option>
                    {/each}
                </select>
            </div>
            <div class="grid gap-2">
                <Label for="proxy-hostname">Public hostname</Label>
                <Input id="proxy-hostname" bind:value={hostname} placeholder="panel.example.com" />
            </div>
            <label class="flex items-center gap-2">
                <input type="checkbox" bind:checked={includePatchserver} />
                Also proxy the patchserver
            </label>
            <Button onclick={() => void render()} disabled={busy || hostname.trim() === ""}
                >{busy ? "Rendering…" : "Render the config"}</Button
            >
            {#if rendered}
                <div class="flex items-center justify-between">
                    <p class="text-muted-foreground">For {rendered.product} at {rendered.hostname}</p>
                    <Button variant="outline" onclick={() => void copyConfig()}>Copy</Button>
                </div>
                <pre class="max-h-96 overflow-auto rounded-md border bg-muted/30 p-3 font-mono text-xs">{rendered.config}</pre>
            {/if}
        </Card.Content>
    </Card.Root>
</div>
