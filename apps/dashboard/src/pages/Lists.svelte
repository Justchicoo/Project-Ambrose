<!-- Project Ambrose by Imjustchico: The list pages that are one table and a few actions each: players online, accounts and bans, realms, backups, panel users and activity, with a search box where the list is long. -->
<script lang="ts">
    import * as Card from "$lib/components/ui/card/index.js";
    import * as Table from "$lib/components/ui/table/index.js";
    import { Button } from "$lib/components/ui/button/index.js";
    import { Input } from "$lib/components/ui/input/index.js";
    import DownloadIcon from "@lucide/svelte/icons/download";
    import PlusIcon from "@lucide/svelte/icons/plus";
    import RefreshCwIcon from "@lucide/svelte/icons/refresh-cw";
    import SearchIcon from "@lucide/svelte/icons/search";
    import ShieldBanIcon from "@lucide/svelte/icons/shield-ban";
    import type { Component } from "svelte";
    import PageHeader from "../components/PageHeader.svelte";
    import StatusBadge from "../components/StatusBadge.svelte";
    import { accounts, activity, backups, panelUsers, players, realms } from "../sample";

    type Props = { which: string; title: string };
    let { which, title }: Props = $props();

    type Row = Record<string, string>;
    type Column = { id: string; header: string; mono?: boolean; status?: boolean; hideBelow?: "sm" | "md" | "lg" };
    type Page = {
        description: string;
        rows: Row[];
        columns: Column[];
        actions: { label: string; icon: Component; primary?: boolean }[];
        search?: string;
    };

    const pages: Record<string, Page> = {
        players: {
            description: "Wizards in the world right now.",
            rows: players,
            columns: [
                { id: "name", header: "Wizard" },
                { id: "level", header: "Level" },
                { id: "school", header: "School", hideBelow: "sm" },
                { id: "zone", header: "Zone", hideBelow: "md" },
                { id: "session", header: "Online for", hideBelow: "lg" },
            ],
            actions: [{ label: "Message everyone", icon: PlusIcon, primary: true }],
            search: "Find a wizard",
        },
        accounts: {
            description: "Game accounts, their security level and any ban.",
            rows: accounts,
            columns: [
                { id: "username", header: "Account" },
                { id: "level", header: "Security level", hideBelow: "sm" },
                { id: "created", header: "Created", hideBelow: "md" },
                { id: "lastSeen", header: "Last seen", hideBelow: "lg" },
                { id: "state", header: "State", status: true },
            ],
            actions: [
                { label: "Ban", icon: ShieldBanIcon },
                { label: "New account", icon: PlusIcon, primary: true },
            ],
            search: "Find an account by name, address or machine",
        },
        realms: {
            description: "Every realm in the realm list and how full it is.",
            rows: realms,
            columns: [
                { id: "name", header: "Realm" },
                { id: "address", header: "Address", mono: true, hideBelow: "md" },
                { id: "flags", header: "Flags", hideBelow: "sm" },
                { id: "population", header: "Population" },
                { id: "heartbeat", header: "Heartbeat", hideBelow: "lg" },
            ],
            actions: [{ label: "Refresh", icon: RefreshCwIcon }],
        },
        backups: {
            description: "Backups kept on this machine, checked after they are taken.",
            rows: backups,
            columns: [
                { id: "name", header: "Backup", mono: true },
                { id: "taken", header: "Taken", hideBelow: "sm" },
                { id: "size", header: "Size", hideBelow: "md" },
                { id: "verified", header: "Check", status: true },
                { id: "kept", header: "Kept for", hideBelow: "lg" },
            ],
            actions: [
                { label: "Download", icon: DownloadIcon },
                { label: "Back up now", icon: PlusIcon, primary: true },
            ],
        },
        users: {
            description: "People who can sign in to this panel and what they may do.",
            rows: panelUsers,
            columns: [
                { id: "name", header: "User" },
                { id: "role", header: "Role" },
                { id: "twoFactor", header: "Two-factor", status: true, hideBelow: "sm" },
                { id: "lastSignIn", header: "Last sign-in", hideBelow: "md" },
            ],
            actions: [{ label: "Invite", icon: PlusIcon, primary: true }],
        },
        activity: {
            description: "What people did in the panel, newest first.",
            rows: activity,
            columns: [
                { id: "when", header: "When", mono: true },
                { id: "who", header: "Who" },
                { id: "what", header: "What" },
                { id: "where", header: "Where", hideBelow: "md" },
            ],
            actions: [{ label: "Export", icon: DownloadIcon }],
        },
    };

    const page = $derived(pages[which]);
    const hide: Record<string, string> = { sm: "hidden sm:table-cell", md: "hidden md:table-cell", lg: "hidden lg:table-cell" };

    function tone(value: string) {
        if (/^(Active|Verified|On)$/.test(value)) return "healthy" as const;
        if (/Banned|Failed|Off/.test(value)) return "wrong" as const;
        return "unknown" as const;
    }
</script>

{#if page}
    <PageHeader {title} description={page.description}>
        {#snippet actions()}
            {#each page.actions as action (action.label)}
                <Button variant={action.primary ? "default" : "outline"}><action.icon />{action.label}</Button>
            {/each}
        {/snippet}
    </PageHeader>
    {#if page.search}
        <div class="relative max-w-md">
            <SearchIcon class="absolute top-1/2 left-2.5 size-4 -translate-y-1/2 text-muted-foreground" />
            <Input type="search" placeholder={page.search} class="pl-8" aria-label={page.search} />
        </div>
    {/if}
    <Card.Root class="py-0 shadow-xs">
        <Table.Root>
            <Table.Header>
                <Table.Row class="hover:bg-transparent">
                    {#each page.columns as column, index (column.id)}
                        <Table.Head class={`${index === 0 ? "pl-6" : ""} ${column.hideBelow ? hide[column.hideBelow] : ""}`}
                            >{column.header}</Table.Head
                        >
                    {/each}
                </Table.Row>
            </Table.Header>
            <Table.Body>
                {#each page.rows as row, rowIndex (rowIndex)}
                    <Table.Row>
                        {#each page.columns as column, index (column.id)}
                            <Table.Cell
                                class={`${index === 0 ? "pl-6 font-medium" : ""} ${column.mono ? "font-mono text-xs" : ""} ${column.hideBelow ? hide[column.hideBelow] : ""}`}
                            >
                                {#if column.status}
                                    <StatusBadge tone={tone(row[column.id] ?? "")}>{row[column.id]}</StatusBadge>
                                {:else}
                                    {row[column.id]}
                                {/if}
                            </Table.Cell>
                        {/each}
                    </Table.Row>
                {/each}
            </Table.Body>
        </Table.Root>
    </Card.Root>
{/if}
