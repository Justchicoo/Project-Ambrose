<!-- Project Ambrose by Imjustchico: A typed property tree that expands nested objects, pages long containers on demand, and names flags and enum options. -->
<script lang="ts">
    import { SvelteSet } from "svelte/reactivity";
    import { classes } from "../internal/classes";
    import ChevronRightIcon from "@lucide/svelte/icons/chevron-right";

    export type TreeNode = {
        name: string;
        type: string;
        detail?: string;
        flags?: string[];
        enumOptions?: Array<{ name: string; value: number }>;
        children?: TreeNode[];
        totalChildren?: number;
        loadMore?: () => Promise<TreeNode[]>;
    };

    type Props = {
        nodes: TreeNode[];
        pageSize?: number;
        class?: string;
    };

    let { nodes, pageSize = 25, class: extra }: Props = $props();

    const expanded = new SvelteSet<string>();
    const loaded = $state(new Map<string, TreeNode[]>());
    const loading = new SvelteSet<string>();

    function keyOf(path: string, name: string): string {
        return `${path}/${name}`;
    }

    function toggle(key: string): void {
        if (expanded.has(key)) expanded.delete(key);
        else expanded.add(key);
    }

    async function more(key: string, node: TreeNode): Promise<void> {
        if (!node.loadMore || loading.has(key)) return;
        loading.add(key);
        try {
            const next = await node.loadMore();
            const prior = loaded.get(key) ?? [];
            loaded.set(key, [...prior, ...next]);
        } finally {
            loading.delete(key);
        }
    }

    function shownChildren(key: string, node: TreeNode): TreeNode[] {
        const extra = loaded.get(key) ?? [];
        const base = node.children ?? [];
        return [...base, ...extra];
    }

    function hasMore(key: string, node: TreeNode): boolean {
        const total = node.totalChildren ?? node.children?.length ?? 0;
        return shownChildren(key, node).length < total;
    }
</script>

{#snippet row(node: TreeNode, path: string, depth: number)}
    {@const key = keyOf(path, node.name)}
    {@const kids = shownChildren(key, node)}
    {@const open = expanded.has(key)}
    {@const expandable = kids.length > 0 || node.loadMore !== undefined}
    <div role="treeitem" aria-expanded={expandable ? open : undefined} aria-level={depth + 1} aria-selected="false" class="select-text">
        <div class={classes("flex items-baseline gap-8 py-2", depth > 0 && "ml-16")}>
            {#if expandable}
                <button
                    type="button"
                    onclick={() => toggle(key)}
                    aria-label={open ? `Collapse ${node.name}` : `Expand ${node.name}`}
                    class="inline-flex size-16 shrink-0 items-center justify-center rounded-input text-fg-muted hover:bg-surface-sunken"
                >
                    <ChevronRightIcon class={classes("size-12 transition-transform", open && "rotate-90")} />
                </button>
            {:else}
                <span class="inline-block size-16 shrink-0" aria-hidden="true"></span>
            {/if}
            <span class="font-mono text-13 font-medium text-fg-body">{node.name}</span>
            <span class="font-mono text-12 text-fg-muted">{node.type}</span>
            {#if node.detail}
                <span class="font-mono text-12 text-fg-faint">{node.detail}</span>
            {/if}
            {#if node.flags?.length}
                <span class="flex flex-wrap gap-4">
                    {#each node.flags as flag (flag)}
                        <span
                            class="rounded-input bg-fg-muted/10 px-6 py-2 font-mono text-11 text-fg-muted ring-1 ring-inset ring-edge-strong/60"
                            >{flag}</span
                        >
                    {/each}
                </span>
            {/if}
        </div>
        {#if node.enumOptions?.length && open}
            <div class={classes("ml-32 border-l border-edge-strong/40 pl-12", depth > 0 && "ml-48")}>
                {#each node.enumOptions as option (option.name)}
                    <div class="flex items-baseline gap-8 py-1 font-mono text-12">
                        <span class="text-fg-body">{option.name}</span>
                        <span class="text-fg-muted">= {option.value}</span>
                    </div>
                {/each}
            </div>
        {/if}
        {#if open && kids.length > 0}
            <div role="group" class={classes("border-l border-edge-strong/40", depth > 0 && "ml-24")}>
                {#each kids as child (child.name)}
                    {@render row(child, key, depth + 1)}
                {/each}
            </div>
        {/if}
        {#if open && hasMore(key, node)}
            <div class={classes("ml-32", depth > 0 && "ml-48")}>
                <button
                    type="button"
                    onclick={() => more(key, node)}
                    disabled={loading.has(key)}
                    class="mt-2 rounded-input px-8 py-4 text-12 font-medium text-fill-action hover:bg-fill-action/10 disabled:opacity-50"
                >
                    {loading.has(key) ? "Loading…" : `Show ${pageSize} more`}
                </button>
            </div>
        {/if}
    </div>
{/snippet}

<div role="tree" aria-label="Typed object tree" class={classes("font-mono", extra)}>
    {#each nodes as node (node.name)}
        {@render row(node, "", 0)}
    {/each}
</div>
