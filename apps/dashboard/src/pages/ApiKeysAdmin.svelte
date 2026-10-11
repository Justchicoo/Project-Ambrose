<!-- Project Ambrose by Imjustchico: Reviewing every operator's API keys for holders of apikeys.manage: whose key it is, what it may do and from where, when it was last used, and revoking any of them so the next request carrying it is refused. -->
<script lang="ts">
    import * as Card from "$lib/components/ui/card/index.js";
    import * as Dialog from "$lib/components/ui/dialog/index.js";
    import * as Table from "$lib/components/ui/table/index.js";
    import { Button } from "$lib/components/ui/button/index.js";
    import { Badge } from "$lib/components/ui/badge/index.js";
    import { ApiError } from "$lib/api.svelte.js";
    import { listAllApiKeys, revokeAnyApiKey, type ApiKey } from "$lib/apikeys.js";
    import { onMount } from "svelte";
    import PageHeader from "../components/PageHeader.svelte";

    let keys = $state<ApiKey[]>([]);
    let loading = $state(true);
    let failure = $state("");
    let notice = $state("");
    let revoking = $state<ApiKey | null>(null);
    let askRevoke = $state(false);
    let busy = $state(false);

    function when(epochMs: number | null): string {
        if (epochMs === null) return "Never";
        const date = new Date(epochMs);
        return Number.isNaN(date.getTime()) ? "Unknown time" : date.toLocaleString();
    }

    async function refresh() {
        loading = true;
        failure = "";
        try {
            keys = await listAllApiKeys();
        } catch (problem) {
            failure = problem instanceof ApiError ? problem.message : "The API keys could not be read.";
        } finally {
            loading = false;
        }
    }

    async function revoke() {
        if (revoking === null || busy) return;
        busy = true;
        failure = "";
        try {
            await revokeAnyApiKey(revoking.public_id);
            notice = `The key "${revoking.name}" of ${revoking.owner_username ?? "its owner"} was revoked.`;
            revoking = null;
            askRevoke = false;
            await refresh();
        } catch (problem) {
            failure = problem instanceof ApiError ? problem.message : "The key could not be revoked.";
        } finally {
            busy = false;
        }
    }

    onMount(() => {
        void refresh();
    });
</script>

<PageHeader title="API key review" description="Every operator's keys, for holders of apikeys.manage.">
    {#snippet actions()}
        <Button variant="outline" onclick={() => void refresh()} disabled={loading}>Refresh</Button>
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
        <Card.Title>All keys</Card.Title>
        <Card.Description>Secrets are never shown here either; the panel keeps only their hashes.</Card.Description>
    </Card.Header>
    <Card.Content>
        {#if loading}
            <p class="py-6 text-sm text-muted-foreground" aria-busy="true">Reading every key…</p>
        {:else if keys.length === 0}
            <p class="py-6 text-sm text-muted-foreground">No operator holds an API key.</p>
        {:else}
            <Table.Root>
                <Table.Header>
                    <Table.Row>
                        <Table.Head>Owner</Table.Head>
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
                            <Table.Cell class="font-medium">{key.owner_username ?? key.owner_user_id}</Table.Cell>
                            <Table.Cell>{key.name}</Table.Cell>
                            <Table.Cell><code class="font-mono text-xs">amb_{key.public_id}</code></Table.Cell>
                            <Table.Cell class="text-xs text-muted-foreground">
                                {key.permissions.length} permission(s) · {key.scopes.join(", ")}
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
                                {#if key.live}
                                    <Button
                                        variant="ghost"
                                        size="sm"
                                        disabled={busy}
                                        onclick={() => {
                                            revoking = key;
                                            askRevoke = true;
                                        }}>Revoke</Button
                                    >
                                {/if}
                            </Table.Cell>
                        </Table.Row>
                    {/each}
                </Table.Body>
            </Table.Root>
        {/if}
    </Card.Content>
</Card.Root>

<Dialog.Root bind:open={askRevoke}>
    <Dialog.Content>
        <Dialog.Header>
            <Dialog.Title>Revoke {revoking?.name ?? "key"}?</Dialog.Title>
            <Dialog.Description>
                This ends {revoking?.owner_username ?? "the owner"}'s key. The next request carrying it is refused at once, and the
                revocation is audited.
            </Dialog.Description>
        </Dialog.Header>
        <Dialog.Footer>
            <Button variant="outline" onclick={() => (askRevoke = false)}>Keep it</Button>
            <Button variant="destructive" onclick={() => void revoke()} disabled={busy}>{busy ? "Revoking…" : "Revoke"}</Button>
        </Dialog.Footer>
    </Dialog.Content>
</Dialog.Root>
