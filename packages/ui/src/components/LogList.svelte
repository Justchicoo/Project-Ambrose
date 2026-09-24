<!-- Project Ambrose by Imjustchico: Log records as real rows in a virtualised list, with the level in the meaning colours and any ANSI in the message turned into text nodes rather than markup. -->
<script lang="ts">
    import anser from "anser";
    import { VList } from "virtua/svelte";
    import { classes } from "../internal/classes";
    import CollectionState from "./CollectionState.svelte";

    type Level = "trace" | "debug" | "info" | "warn" | "error" | "fatal";

    type LogRecord = {
        sequence: number;
        time: string;
        level: Level;
        category: string;
        message: string;
    };

    type Props = {
        label: string;
        records: LogRecord[];
        height?: string;
        status?: "ready" | "loading" | "no-results" | "error";
        empty?: string;
        noResults?: string;
        error?: string;
        class?: string;
    };

    let {
        label,
        records,
        height = "20rem",
        status = "ready",
        empty = "Nothing yet",
        noResults = "Nothing matched",
        error = "Could not be loaded",
        class: extra,
    }: Props = $props();

    let placeholder = $derived(status === "ready" ? (records.length === 0 ? ("empty" as const) : null) : status);

    const levels: Record<Level, string> = {
        trace: "text-fg-faint",
        debug: "text-fg-faint",
        info: "text-fg-muted",
        warn: "text-state-waiting",
        error: "text-state-wrong",
        fatal: "text-state-wrong",
    };

    function pieces(message: string): string[] {
        return anser.ansiToJson(message, { json: true, remove_empty: true }).map((entry) => entry.content);
    }
</script>

{#if placeholder}
    <CollectionState status={placeholder} {label} {empty} {noResults} {error} class={extra} />
{:else}
    <div
        aria-label={label}
        role="log"
        style="height: {height}"
        class={classes("ambrose-mono rounded-card border border-edge-quiet bg-surface-sunken text-12", extra)}
        data-ambrose-scroll
    >
        <VList data={records} getKey={(record: LogRecord) => record.sequence} style="height: 100%" tabindex={0}>
            {#snippet children(record: LogRecord)}
                <div class="flex items-start gap-12 px-12 py-4">
                    <span class="shrink-0 text-fg-faint">{record.time}</span>
                    <span class={classes("w-44 shrink-0 uppercase", levels[record.level])}>{record.level}</span>
                    <span class="shrink-0 text-fg-faint">{record.category}</span>
                    <span class="min-w-0 break-all text-fg-body">
                        {#each pieces(record.message) as piece, index (index)}<span>{piece}</span>{/each}
                    </span>
                </div>
            {/snippet}
        </VList>
    </div>
{/if}
