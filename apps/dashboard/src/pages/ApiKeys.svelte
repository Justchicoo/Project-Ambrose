<!-- Project Ambrose by Imjustchico: The operator's own API keys: every key they hold with when it was made, when it expires and when it was last used, making a new one with a permission subset no wider than their own rights, scopes and allowed CIDRs, the secret shown once in a dialog that only the operator's own button closes, rotating a key with an optional short grace for the old secret, and revoking one so the next request carrying it is refused. -->
<script lang="ts">
    import * as Card from "$lib/components/ui/card/index.js";
    import * as Dialog from "$lib/components/ui/dialog/index.js";
    import * as Table from "$lib/components/ui/table/index.js";
    import { Button } from "$lib/components/ui/button/index.js";
    import { Input } from "$lib/components/ui/input/index.js";
    import { Label } from "$lib/components/ui/label/index.js";
    import { Textarea } from "$lib/components/ui/textarea/index.js";
    import { Badge } from "$lib/components/ui/badge/index.js";
    import { ApiError, session } from "$lib/api.svelte.js";
    import {
        createApiKey,
        listApiKeys,
        permissionCatalog,
        revokeApiKey,
        rotateApiKey,
        type ApiKey,
        type ApiKeyCreated,
        type CatalogPermission,
    } from "$lib/apikeys.js";
    import { onMount } from "svelte";
    import PageHeader from "../components/PageHeader.svelte";

    let keys = $state<ApiKey[]>([]);
    let catalog = $state<CatalogPermission[]>([]);
    let loading = $state(true);
    let failure = $state("");
    let notice = $state("");

    let creating = $state(false);
    let name = $state("");
    let picked = $state<string[]>([]);
    let scopes = $state("panel");
    let cidrs = $state("");
    let expiryDays = $state("90");
    let noExpiry = $state(false);
    let busy = $state(false);
    let formFailure = $state("");

    let shown = $state<ApiKeyCreated | null>(null);
    let showSecret = $state(false);
    let rotating = $state<ApiKey | null>(null);
    let askRotate = $state(false);
    let graceSeconds = $state("300");
    let revoking = $state<ApiKey | null>(null);
    let askRevoke = $state(false);
    let copied = $state(false);

    const held = $derived(new Set(session.user?.permissions ?? []));
    const grouped = $derived.by(() => {
        const groups = new Map<string, CatalogPermission[]>();
        for (const permission of catalog) {
            if (!held.has("*") && !held.has(permission.key)) continue;
            const list = groups.get(permission.group) ?? [];
            list.push(permission);
            groups.set(permission.group, list);
        }
        return [...groups.entries()].sort((left, right) => left[0].localeCompare(right[0]));
    });

    function when(epochMs: number | null): string {
        if (epochMs === null) return "Never";
        const date = new Date(epochMs);
        return Number.isNaN(date.getTime()) ? "Unknown time" : date.toLocaleString();
    }

    function describe(error: ApiError): string {
        if (error.code === "too_many_keys") return "You already hold the most keys the panel allows. Revoke one first.";
        if (error.status === 422) return Object.values(error.fields)[0] ?? error.message;
        return error.message;
    }

    async function refresh() {
        loading = true;
        failure = "";
        try {
            keys = await listApiKeys();
        } catch (problem) {
            failure = problem instanceof ApiError ? problem.message : "Your API keys could not be read.";
        } finally {
            loading = false;
        }
    }

    function togglePermission(key: string, checked: boolean) {
        picked = checked ? [...new Set([...picked, key])] : picked.filter((entry) => entry !== key);
    }

    function resetForm() {
        name = "";
        picked = [];
        scopes = "panel";
        cidrs = "";
        expiryDays = "90";
        noExpiry = false;
        formFailure = "";
    }

    async function make() {
        if (busy || name.trim() === "") return;
        busy = true;
        formFailure = "";
        try {
            const draft: Parameters<typeof createApiKey>[0] = {
                name: name.trim(),
                permissions: picked,
                scopes: scopes
                    .split(",")
                    .map((entry) => entry.trim())
                    .filter((entry) => entry !== ""),
                cidrs: cidrs
                    .split("\n")
                    .map((entry) => entry.trim())
                    .filter((entry) => entry !== ""),
            };
            if (noExpiry) draft.no_expiry = true;
            else if (expiryDays.trim() !== "") draft.expires_epoch_ms = Date.now() + Number(expiryDays) * 86400000;
            const answer = await createApiKey(draft);
            creating = false;
            resetForm();
            copied = false;
            shown = answer;
            showSecret = true;
            notice = "";
            await refresh();
        } catch (problem) {
            formFailure = problem instanceof ApiError ? describe(problem) : "The key could not be made.";
        } finally {
            busy = false;
        }
    }

    async function revoke() {
        if (revoking === null || busy) return;
        busy = true;
        failure = "";
        try {
            await revokeApiKey(revoking.public_id);
            notice = `The key "${revoking.name}" was revoked; the next request carrying it is refused.`;
            revoking = null;
            askRevoke = false;
            await refresh();
        } catch (problem) {
            failure = problem instanceof ApiError ? problem.message : "The key could not be revoked.";
        } finally {
            busy = false;
        }
    }

    async function rotate() {
        if (rotating === null || busy) return;
        const grace = Number(graceSeconds);
        if (!Number.isInteger(grace) || grace < 0 || grace > 3600) {
            formFailure = "Give a grace of 0 to 3600 seconds.";
            return;
        }
        busy = true;
        formFailure = "";
        try {
            const answer = await rotateApiKey(rotating.public_id, grace);
            rotating = null;
            askRotate = false;
            copied = false;
            shown = answer;
            showSecret = true;
            notice = "";
            await refresh();
        } catch (problem) {
            formFailure = problem instanceof ApiError ? describe(problem) : "The key could not be rotated.";
        } finally {
            busy = false;
        }
    }

    async function copyKey() {
        if (shown === null) return;
        try {
            await navigator.clipboard.writeText(shown.key);
            copied = true;
        } catch {
            copied = false;
        }
    }

    onMount(() => {
        void (async () => {
            try {
                catalog = (await permissionCatalog()).permissions;
            } catch {
                catalog = [];
            }
            await refresh();
        })();
    });
</script>

<PageHeader title="API keys" description="Scoped, expiring keys that automate the panel with no more rights than you hold.">
    {#snippet actions()}
        <Button
            onclick={() => {
                resetForm();
                creating = true;
            }}>New API key</Button
        >
    {/snippet}
</PageHeader>

{#if failure}
    <p class="rounded-md border border-destructive/30 bg-destructive/5 p-3 text-sm text-destructive" role="alert">{failure}</p>
{/if}
{#if notice}
    <p class="rounded-md border border-healthy/30 bg-healthy/5 p-3 text-sm text-healthy" role="status">{notice}</p>
{/if}

<Card.Root>
    <Card.Header>
        <Card.Title>Your keys</Card.Title>
        <Card.Description>The secret is shown once, when a key is made. Everything else about a key can be read here.</Card.Description>
    </Card.Header>
    <Card.Content>
        {#if loading}
            <p class="py-6 text-sm text-muted-foreground" aria-busy="true">Reading your keys…</p>
        {:else if keys.length === 0}
            <p class="py-6 text-sm text-muted-foreground">You hold no API keys yet.</p>
        {:else}
            <Table.Root>
                <Table.Header>
                    <Table.Row>
                        <Table.Head>Name</Table.Head>
                        <Table.Head>Public id</Table.Head>
                        <Table.Head>Permissions</Table.Head>
                        <Table.Head>Expires</Table.Head>
                        <Table.Head>Last used</Table.Head>
                        <Table.Head>Status</Table.Head>
                        <Table.Head><span class="sr-only">Actions</span></Table.Head>
                    </Table.Row>
                </Table.Header>
                <Table.Body>
                    {#each keys as key (key.public_id)}
                        <Table.Row>
                            <Table.Cell class="font-medium">{key.name}</Table.Cell>
                            <Table.Cell><code class="font-mono text-xs">amb_{key.public_id}</code></Table.Cell>
                            <Table.Cell class="text-xs text-muted-foreground">
                                {key.permissions.length} permission(s) · {key.scopes.join(", ")}
                                {#if key.cidrs.length > 0}
                                    <span class="block">CIDRs: {key.cidrs.join(", ")}</span>
                                {/if}
                            </Table.Cell>
                            <Table.Cell class="text-xs">{key.expires_epoch_ms === null ? "Never" : when(key.expires_epoch_ms)}</Table.Cell>
                            <Table.Cell class="text-xs">
                                {when(key.last_used_epoch_ms)}
                                {#if key.last_used_address}
                                    <span class="block text-muted-foreground">from {key.last_used_address}</span>
                                {/if}
                            </Table.Cell>
                            <Table.Cell>
                                <Badge variant={key.live ? "default" : "secondary"}>{key.live ? "Live" : "Revoked"}</Badge>
                            </Table.Cell>
                            <Table.Cell>
                                <div class="flex gap-1">
                                    {#if key.live}
                                        <Button
                                            variant="ghost"
                                            size="sm"
                                            disabled={busy}
                                            onclick={() => {
                                                rotating = key;
                                                askRotate = true;
                                                formFailure = "";
                                                graceSeconds = "300";
                                            }}
                                        >
                                            Rotate
                                        </Button>
                                        <Button
                                            variant="ghost"
                                            size="sm"
                                            disabled={busy}
                                            onclick={() => {
                                                revoking = key;
                                                askRevoke = true;
                                            }}>Revoke</Button>
                                    {/if}
                                </div>
                            </Table.Cell>
                        </Table.Row>
                    {/each}
                </Table.Body>
            </Table.Root>
        {/if}
    </Card.Content>
</Card.Root>

<Dialog.Root bind:open={creating}>
    <Dialog.Content class="sm:max-w-2xl">
        <Dialog.Header>
            <Dialog.Title>New API key</Dialog.Title>
            <Dialog.Description>
                The key can never do more than you can: its subset is checked against your own rights when it is made, and against
                your current rights on every request, so losing a permission shrinks every key at once.
            </Dialog.Description>
        </Dialog.Header>
        <div class="space-y-4">
            {#if formFailure}
                <p class="rounded-md border border-destructive/30 bg-destructive/5 p-3 text-sm text-destructive" role="alert">{formFailure}</p>
            {/if}
            <div class="space-y-2">
                <Label for="key-name">Name</Label>
                <Input id="key-name" bind:value={name} maxlength={64} placeholder="Nightly backup script" />
            </div>
            <div class="space-y-2">
                <Label>Permission subset</Label>
                {#if grouped.length === 0}
                    <p class="text-xs text-muted-foreground">The permission catalog could not be read.</p>
                {:else}
                    <div class="max-h-64 space-y-4 overflow-auto rounded-md border p-3">
                        {#each grouped as [group, permissions] (group)}
                            <div>
                                <p class="mb-1 text-xs font-medium text-muted-foreground">{group}</p>
                                <div class="space-y-1">
                                    {#each permissions as permission (permission.key)}
                                        <label class="flex cursor-pointer items-start gap-2 text-sm">
                                            <input
                                                type="checkbox"
                                                class="mt-0.5 size-4 rounded border-input accent-primary"
                                                checked={picked.includes(permission.key)}
                                                onchange={(event) => togglePermission(permission.key, event.currentTarget.checked)}
                                            />
                                            <span>
                                                <code class="font-mono text-xs">{permission.key}</code>
                                                <span class="block text-xs text-muted-foreground">{permission.description}</span>
                                            </span>
                                        </label>
                                    {/each}
                                </div>
                            </div>
                        {/each}
                    </div>
                {/if}
            </div>
            <div class="grid gap-4 sm:grid-cols-2">
                <div class="space-y-2">
                    <Label for="key-scopes">Scopes</Label>
                    <Input id="key-scopes" bind:value={scopes} placeholder="panel" />
                    <p class="text-xs text-muted-foreground">Comma-separated: panel, or an app name for its relayed admin API.</p>
                </div>
                <div class="space-y-2">
                    <Label for="key-cidrs">Allowed CIDRs</Label>
                    <Textarea id="key-cidrs" bind:value={cidrs} rows={3} placeholder={"10.0.0.0/8\n2001:db8::/32"} />
                    <p class="text-xs text-muted-foreground">One address or CIDR range per line, IPv4 or IPv6. Empty allows everywhere.</p>
                </div>
            </div>
            <div class="grid gap-4 sm:grid-cols-2">
                <div class="space-y-2">
                    <Label for="key-expiry">Expires in (days)</Label>
                    <Input id="key-expiry" type="number" min="1" bind:value={expiryDays} disabled={noExpiry} />
                </div>
                <div class="flex items-end gap-2 pb-2">
                    <input id="key-no-expiry" type="checkbox" class="size-4 rounded border-input accent-primary" bind:checked={noExpiry} />
                    <Label for="key-no-expiry">Never expires</Label>
                </div>
            </div>
            <p class="text-xs text-muted-foreground">A key that never expires is made only when the panel's policy allows it.</p>
        </div>
        <Dialog.Footer>
            <Button
                variant="outline"
                onclick={() => {
                    creating = false;
                    resetForm();
                }}>Cancel</Button
            >
            <Button onclick={() => void make()} disabled={busy || name.trim() === ""}>{busy ? "Making…" : "Make key"}</Button>
        </Dialog.Footer>
    </Dialog.Content>
</Dialog.Root>

<Dialog.Root bind:open={showSecret}>
    <Dialog.Content
        showCloseButton={false}
        escapeKeydownBehavior="ignore"
        onInteractOutside={(event) => event.preventDefault()}
    >
        <Dialog.Header>
            <Dialog.Title>Copy this key now</Dialog.Title>
            <Dialog.Description>
                This is the only time the secret is shown. The panel keeps only its hash, so it cannot be read again from anywhere.
            </Dialog.Description>
        </Dialog.Header>
        {#if shown !== null}
            <div class="space-y-3">
                <code class="block rounded-md border bg-muted/30 p-4 font-mono text-xs break-all select-all">{shown.key}</code>
                <div class="flex gap-2">
                    <Button variant="outline" onclick={() => void copyKey()}>{copied ? "Copied" : "Copy"}</Button>
                    <Button
                        onclick={() => {
                            showSecret = false;
                            shown = null;
                            notice = "The key was made. Its secret was shown once and cannot be read again.";
                        }}>I have saved it</Button
                    >
                </div>
            </div>
        {/if}
    </Dialog.Content>
</Dialog.Root>

<Dialog.Root bind:open={askRotate}>
    <Dialog.Content>
        <Dialog.Header>
            <Dialog.Title>Rotate {rotating?.name ?? "key"}</Dialog.Title>
            <Dialog.Description>
                A replacement is made with the same permissions, scopes and CIDRs. The old secret keeps working for the grace you give
                it, then stops.
            </Dialog.Description>
        </Dialog.Header>
        <div class="space-y-4">
            {#if formFailure}
                <p class="rounded-md border border-destructive/30 bg-destructive/5 p-3 text-sm text-destructive" role="alert">{formFailure}</p>
            {/if}
            <div class="space-y-2">
                <Label for="rotate-grace">Grace for the old secret (seconds)</Label>
                <Input id="rotate-grace" type="number" min="0" max="3600" bind:value={graceSeconds} />
            </div>
        </div>
        <Dialog.Footer>
            <Button variant="outline" onclick={() => (askRotate = false)}>Cancel</Button>
            <Button onclick={() => void rotate()} disabled={busy}>{busy ? "Rotating…" : "Rotate"}</Button>
        </Dialog.Footer>
    </Dialog.Content>
</Dialog.Root>

<Dialog.Root bind:open={askRevoke}>
    <Dialog.Content>
        <Dialog.Header>
            <Dialog.Title>Revoke {revoking?.name ?? "key"}?</Dialog.Title>
            <Dialog.Description>The next request carrying this key is refused at once.</Dialog.Description>
        </Dialog.Header>
        <Dialog.Footer>
            <Button variant="outline" onclick={() => (askRevoke = false)}>Keep it</Button>
            <Button variant="destructive" onclick={() => void revoke()} disabled={busy}>{busy ? "Revoking…" : "Revoke"}</Button>
        </Dialog.Footer>
    </Dialog.Content>
</Dialog.Root>
