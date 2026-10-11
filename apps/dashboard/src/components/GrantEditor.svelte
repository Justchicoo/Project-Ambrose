<!-- Project Ambrose by Imjustchico: The grant editor: every permission the panel's catalog holds, drawn by group with its sentence and a danger badge where one is due, a key the editor may not grant shown disabled with the reason why, each group's select-all reaching only the keys the editor may grant, and a review step that lists what will be granted before it is confirmed. -->
<script lang="ts">
    import { permissionCatalog } from "$lib/api.svelte.js";
    import { Badge } from "$lib/components/ui/badge/index.js";
    import { Button } from "$lib/components/ui/button/index.js";
    import * as Card from "$lib/components/ui/card/index.js";
    import { Switch } from "$lib/components/ui/switch/index.js";
    import type { PermissionCatalogAnswer, PermissionCatalogEntry } from "$lib/schemas.js";
    import { onMount } from "svelte";

    type Props = {
        scope: string;
        grantable: Set<string>;
        isOwner: boolean;
        selected?: string[];
        confirmLabel?: string;
        onConfirm?: (keys: string[]) => void;
    };

    let { scope, grantable, isOwner, selected = $bindable([]), confirmLabel = "Review", onConfirm }: Props = $props();

    let catalog = $state<PermissionCatalogAnswer | null>(null);
    let loading = $state(true);
    let failure = $state("");
    let reviewing = $state(false);

    const chosen = $derived(new Set(selected));

    type Group = { name: string; entries: PermissionCatalogEntry[] };
    const groups = $derived.by((): Group[] => {
        if (!catalog) return [];
        const ordered: Group[] = [];
        const byName = new Map<string, Group>();
        for (const entry of catalog.permissions) {
            let group = byName.get(entry.group);
            if (!group) {
                group = { name: entry.group, entries: [] };
                byName.set(entry.group, group);
                ordered.push(group);
            }
            group.entries.push(entry);
        }
        return ordered;
    });

    const reviewGroups = $derived.by((): Group[] => {
        const wanted = new Set(selected);
        return groups
            .map((group) => ({ name: group.name, entries: group.entries.filter((entry) => wanted.has(entry.key)) }))
            .filter((group) => group.entries.length > 0);
    });

    function disabledReason(entry: PermissionCatalogEntry): string | null {
        if (entry.owner_only && !isOwner) return "Only the owner can grant this.";
        if (!grantable.has(entry.key)) return "You do not hold this permission.";
        return null;
    }

    function toggle(key: string, on: boolean) {
        const next = new Set(chosen);
        if (on) next.add(key);
        else next.delete(key);
        selected = [...next].sort();
    }

    function selectGroup(group: Group, on: boolean) {
        const next = new Set(chosen);
        for (const entry of group.entries) {
            if (disabledReason(entry) !== null) continue;
            if (on) next.add(entry.key);
            else next.delete(entry.key);
        }
        selected = [...next].sort();
    }

    function groupSelectable(group: Group): PermissionCatalogEntry[] {
        return group.entries.filter((entry) => disabledReason(entry) === null);
    }

    onMount(async () => {
        try {
            catalog = await permissionCatalog();
        } catch {
            failure = "The permission catalog could not be read.";
        } finally {
            loading = false;
        }
    });
</script>

<Card.Root>
    <Card.Header>
        <Card.Title>Grants at {scope}</Card.Title>
        <Card.Description>Only the permissions you hold can be granted. A disabled key names why.</Card.Description>
    </Card.Header>
    <Card.Content>
        {#if loading}
            <p class="text-sm text-muted-foreground">Reading the permission catalog…</p>
        {:else if failure !== ""}
            <p class="text-sm text-destructive">{failure}</p>
        {:else if !reviewing}
            <div class="space-y-6">
                {#each groups as group (group.name)}
                    {@const selectable = groupSelectable(group)}
                    {@const allOn = selectable.length > 0 && selectable.every((entry) => chosen.has(entry.key))}
                    <section>
                        <div class="mb-2 flex items-center justify-between">
                            <h3 class="text-sm font-semibold capitalize">{group.name}</h3>
                            <Button
                                variant="ghost"
                                size="sm"
                                disabled={selectable.length === 0}
                                onclick={() => selectGroup(group, !allOn)}
                            >
                                {allOn ? "Clear group" : "Select all grantable"}
                            </Button>
                        </div>
                        <ul class="space-y-2">
                            {#each group.entries as entry (entry.key)}
                                {@const reason = disabledReason(entry)}
                                <li class="flex items-start justify-between gap-4 rounded-md border p-3">
                                    <div class="min-w-0">
                                        <div class="flex flex-wrap items-center gap-2">
                                            <code class="text-sm font-medium">{entry.key}</code>
                                            {#if entry.danger}
                                                <Badge variant="destructive">danger</Badge>
                                            {/if}
                                            {#if entry.owner_only}
                                                <Badge variant="outline">owner only</Badge>
                                            {/if}
                                        </div>
                                        <p class="mt-1 text-sm text-muted-foreground">{entry.description}</p>
                                        {#if reason !== null}
                                            <p class="mt-1 text-xs text-muted-foreground">{reason}</p>
                                        {/if}
                                    </div>
                                    <Switch
                                        checked={chosen.has(entry.key)}
                                        disabled={reason !== null}
                                        onCheckedChange={(on) => toggle(entry.key, on)}
                                        aria-label={entry.key}
                                    />
                                </li>
                            {/each}
                        </ul>
                    </section>
                {/each}
            </div>
        {:else}
            <div class="space-y-4">
                {#if reviewGroups.length === 0}
                    <p class="text-sm text-muted-foreground">No permissions selected. Nothing will be granted.</p>
                {:else}
                    {#each reviewGroups as group (group.name)}
                        <section>
                            <h3 class="mb-2 text-sm font-semibold capitalize">{group.name}</h3>
                            <ul class="space-y-1">
                                {#each group.entries as entry (entry.key)}
                                    <li class="flex flex-wrap items-center gap-2 text-sm">
                                        <code class="font-medium">{entry.key}</code>
                                        {#if entry.danger}
                                            <Badge variant="destructive">danger</Badge>
                                        {/if}
                                        <span class="text-muted-foreground">— {entry.description}</span>
                                    </li>
                                {/each}
                            </ul>
                        </section>
                    {/each}
                {/if}
            </div>
        {/if}
    </Card.Content>
    {#if !loading && failure === ""}
        <Card.Footer class="flex justify-end gap-2">
            {#if reviewing}
                <Button variant="outline" onclick={() => (reviewing = false)}>Back</Button>
                <Button onclick={() => onConfirm?.(selected)}>Confirm</Button>
            {:else}
                <Button onclick={() => (reviewing = true)}>{confirmLabel}</Button>
            {/if}
        </Card.Footer>
    {/if}
</Card.Root>
