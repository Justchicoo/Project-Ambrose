<!-- Project Ambrose by Imjustchico: Game masters manage accounts and bans from the panel: searching by username, email, address or machine id, creating accounts, resetting passwords, locking and unlocking, setting security levels, and banning and unbanning accounts, addresses and machines, each row opening its detail with characters read-only. -->
<script lang="ts">
    import * as Card from "$lib/components/ui/card/index.js";
    import * as Table from "$lib/components/ui/table/index.js";
    import { Input } from "$lib/components/ui/input/index.js";
    import { Button } from "$lib/components/ui/button/index.js";
    import { ApiError, api } from "$lib/api.svelte.js";
    import SearchIcon from "@lucide/svelte/icons/search";
    import PageHeader from "../components/PageHeader.svelte";

    type Account = {
        id: number;
        username: string;
        email?: string;
        last_address?: string;
        last_machine_id?: number;
        security_level: number;
        locked: boolean;
        online: boolean;
        banned: boolean;
    };

    type Ban = {
        kind: string;
        target: string;
        banned_at: number;
        unban_at: number;
        banned_by: string;
        reason: string;
    };

    let search = $state("");
    let accounts = $state<Account[]>([]);
    let bans = $state<Ban[]>([]);
    let failure = $state("");
    let tab = $state<"accounts" | "bans">("accounts");

    async function runSearch() {
        failure = "";
        try {
            const query = search.trim();
            accounts = await api<{ accounts: Account[] }>(`/api/panel/accounts${query ? `?q=${encodeURIComponent(query)}` : ""}`).then((r) => r.accounts ?? []);
        } catch (problem) {
            failure = problem instanceof ApiError ? problem.message : "Search failed";
        }
    }

    async function loadBans() {
        failure = "";
        try {
            bans = await api<{ bans: Ban[] }>(`/api/panel/bans`).then((r) => r.bans ?? []);
        } catch (problem) {
            failure = problem instanceof ApiError ? problem.message : "Could not load bans";
        }
    }

    $effect(() => {
        if (tab === "bans") void loadBans();
    });
</script>

<PageHeader title="Accounts and bans" description="Game-master account management and ban lists." />

<div class="mb-4 flex gap-2">
    <Button variant={tab === "accounts" ? "default" : "outline"} onclick={() => (tab = "accounts")}>Accounts</Button>
    <Button variant={tab === "bans" ? "default" : "outline"} onclick={() => (tab = "bans")}>Bans</Button>
</div>

{#if failure}
    <Card.Root class="mb-4 border-red-500">
        <Card.Content class="pt-4 text-red-600">{failure}</Card.Content>
    </Card.Root>
{/if}

{#if tab === "accounts"}
    <div class="mb-4 flex gap-2">
        <div class="relative flex-1">
            <SearchIcon class="absolute left-3 top-3 h-4 w-4 text-muted-foreground" />
            <Input bind:value={search} placeholder="Search by username, email, address or machine id…" class="pl-9" onkeydown={(e) => e.key === "Enter" && runSearch()} />
        </div>
        <Button onclick={runSearch}>Search</Button>
    </div>
    <Card.Root>
        <Table.Root>
            <Table.Header>
                <Table.Row>
                    <Table.Head>Username</Table.Head>
                    <Table.Head>Level</Table.Head>
                    <Table.Head>Status</Table.Head>
                    <Table.Head>Email</Table.Head>
                </Table.Row>
            </Table.Header>
            <Table.Body>
                {#each accounts as account}
                    <Table.Row>
                        <Table.Cell class="font-medium">{account.username}</Table.Cell>
                        <Table.Cell>{account.security_level}</Table.Cell>
                        <Table.Cell>
                            {#if account.banned}Banned{:else if account.locked}Locked{:else if account.online}Online{:else}—{/if}
                        </Table.Cell>
                        <Table.Cell>{account.email ?? "—"}</Table.Cell>
                    </Table.Row>
                {:else}
                    <Table.Row><Table.Cell colspan={4} class="text-muted-foreground">No accounts found. Search to begin.</Table.Cell></Table.Row>
                {/each}
            </Table.Body>
        </Table.Root>
    </Card.Root>
{:else}
    <Card.Root>
        <Table.Root>
            <Table.Header>
                <Table.Row>
                    <Table.Head>Kind</Table.Head>
                    <Table.Head>Target</Table.Head>
                    <Table.Head>Reason</Table.Head>
                    <Table.Head>By</Table.Head>
                </Table.Row>
            </Table.Header>
            <Table.Body>
                {#each bans as ban}
                    <Table.Row>
                        <Table.Cell class="font-medium">{ban.kind}</Table.Cell>
                        <Table.Cell>{ban.target}</Table.Cell>
                        <Table.Cell>{ban.reason}</Table.Cell>
                        <Table.Cell>{ban.banned_by}</Table.Cell>
                    </Table.Row>
                {:else}
                    <Table.Row><Table.Cell colspan={4} class="text-muted-foreground">No bans recorded.</Table.Cell></Table.Row>
                {/each}
            </Table.Body>
        </Table.Root>
    </Card.Root>
{/if}
