<!-- Project Ambrose by Imjustchico: A real table with a caption and real header cells, sorted by the headless table engine, so a screen reader reads it as a table and the look stays ours. -->
<script lang="ts" generics="Row extends Record<string, unknown>">
    import { createTable } from "@tanstack/svelte-table";
    import {
        createSortedRowModel,
        rowSortingFeature,
        sortFn_alphanumeric,
        sortFn_basic,
        sortFn_text,
        tableFeatures,
        type ColumnDef,
    } from "@tanstack/table-core";
    import { classes } from "../internal/classes";
    import CollectionState from "./CollectionState.svelte";
    import Icon from "./Icon.svelte";

    const features = tableFeatures({
        rowSortingFeature,
        sortedRowModel: createSortedRowModel(),
        sortFns: { alphanumeric: sortFn_alphanumeric, text: sortFn_text, basic: sortFn_basic },
    });

    type Column = {
        id: string;
        header: string;
        value: (row: Row) => string;
        mono?: boolean;
        align?: "start" | "end";
        sortable?: boolean;
    };

    type Props = {
        caption: string;
        columns: Column[];
        rows: Row[];
        empty?: string;
        status?: "ready" | "loading" | "no-results" | "error";
        noResults?: string;
        error?: string;
        class?: string;
    };

    let {
        caption,
        columns,
        rows,
        empty = "No rows",
        status = "ready",
        noResults = "Nothing matched",
        error = "Could not be loaded",
        class: extra,
    }: Props = $props();

    const byId = $derived(new Map(columns.map((column) => [column.id, column])));

    const definitions = $derived(
        columns.map((column) => ({
            id: column.id,
            header: column.header,
            accessorFn: (row: Row) => column.value(row),
            enableSorting: column.sortable !== false,
        })) as ColumnDef<typeof features, Row, unknown>[],
    );

    const table = createTable({
        features,
        get data() {
            return rows;
        },
        get columns() {
            return definitions;
        },
    });

    const sorting = $derived(table.store.get().sorting ?? []);

    function direction(id: string): "ascending" | "descending" | "none" {
        const entry = sorting.find((item) => item.id === id);
        if (!entry) {
            return "none";
        }
        return entry.desc ? "descending" : "ascending";
    }
</script>

{#if status !== "ready"}
    <CollectionState {status} label={caption} {noResults} {error} class={extra} />
{:else}
    <div class={classes("overflow-x-auto rounded-card border border-edge-quiet bg-surface-card", extra)}>
        <table class="w-full border-collapse text-13">
            <caption class="ambrose-label px-16 py-10 text-start">{caption}</caption>
            <thead>
                <tr class="border-b border-edge-quiet">
                    {#each table.getHeaderGroups()[0].headers as header (header.id)}
                        {@const column = byId.get(header.column.id)}
                        <th
                            scope="col"
                            aria-sort={direction(header.column.id) === "none" ? undefined : direction(header.column.id)}
                            class={classes("px-16 py-10", column?.align === "end" ? "text-end" : "text-start")}
                        >
                            {#if column?.sortable !== false}
                                <button
                                    type="button"
                                    class="ambrose-hover ambrose-label inline-flex min-h-44 items-center gap-6 text-fg-faint hover:text-fg-body"
                                    onclick={() => header.column.toggleSorting()}
                                >
                                    {column?.header ?? header.column.id}
                                    {#if direction(header.column.id) !== "none"}
                                        <Icon
                                            name={direction(header.column.id) === "ascending" ? "chevron-down" : "chevron-right"}
                                            size="13"
                                        />
                                    {/if}
                                </button>
                            {:else}
                                <span class="ambrose-label">{column?.header ?? header.column.id}</span>
                            {/if}
                        </th>
                    {/each}
                </tr>
            </thead>
            <tbody>
                {#each table.getRowModel().rows as row (row.id)}
                    <tr class="border-b border-edge-quiet last:border-b-0 odd:bg-surface-sunken">
                        {#each row.getAllCells() as cell (cell.id)}
                            {@const column = byId.get(cell.column.id)}
                            <td
                                class={classes(
                                    "px-16 py-10 text-fg-body",
                                    column?.mono ? "ambrose-mono" : null,
                                    column?.align === "end" ? "text-end" : "text-start",
                                )}
                            >
                                {String(cell.getValue() ?? "")}
                            </td>
                        {/each}
                    </tr>
                {:else}
                    <tr>
                        <td colspan={columns.length} class="px-16 py-20 text-center text-fg-muted">{empty}</td>
                    </tr>
                {/each}
            </tbody>
        </table>
    </div>
{/if}
