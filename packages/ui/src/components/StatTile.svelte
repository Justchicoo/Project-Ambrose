<!-- Project Ambrose by Imjustchico: One number worth watching, as a heading, a monospaced figure and a state word in real markup, so it can be selected, translated and read aloud. -->
<script lang="ts">
    import type { Snippet } from "svelte";
    import { classes } from "../internal/classes";
    import StateDot from "./StateDot.svelte";

    type Props = {
        label: string;
        value: string;
        unit?: string;
        state?: "healthy" | "waiting" | "wrong" | "unknown";
        word?: string;
        class?: string;
        chart?: Snippet;
    };

    let { label, value, unit, state, word, class: extra, chart }: Props = $props();
</script>

<figure class={classes("flex flex-col gap-8 rounded-card border border-edge-quiet bg-surface-card p-16", extra)}>
    <figcaption class="ambrose-label">{label}</figcaption>
    <p class="flex items-baseline gap-6">
        <span class="ambrose-mono text-26 text-fg-body">{value}</span>
        {#if unit}
            <span class="text-12 text-fg-faint">{unit}</span>
        {/if}
    </p>
    {#if state && word}
        <StateDot {state} {word} size="12" />
    {/if}
    {#if chart}
        <div class="pt-4">{@render chart()}</div>
    {/if}
</figure>
