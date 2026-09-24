<!-- Project Ambrose by Imjustchico: A short standing fact about a row, in the meaning colours, as a filled pill when it has to be noticed and a quiet one when it does not. -->
<script lang="ts">
    import type { Snippet } from "svelte";
    import { classes } from "../internal/classes";

    type Tone = "neutral" | "healthy" | "waiting" | "wrong" | "mine";

    type Props = {
        tone?: Tone;
        filled?: boolean;
        class?: string;
        children: Snippet;
    };

    let { tone = "neutral", filled = false, class: extra, children }: Props = $props();

    const quiet: Record<Tone, string> = {
        neutral: "bg-fg-muted/10 text-fg-muted ring-edge-strong/60",
        healthy: "bg-state-healthy/12 text-state-healthy ring-state-healthy/25",
        waiting: "bg-state-waiting/12 text-state-waiting ring-state-waiting/25",
        wrong: "bg-state-wrong/12 text-state-wrong ring-state-wrong/25",
        mine: "bg-mine/12 text-mine ring-mine/25",
    };
    const solid: Record<Tone, string> = {
        neutral: "bg-surface-sunken text-fg-body ring-edge-strong",
        healthy: "bg-fill-healthy text-on-fill ring-fill-healthy",
        waiting: "bg-fill-action text-on-fill ring-fill-action",
        wrong: "bg-fill-danger text-on-fill ring-fill-danger",
        mine: "bg-fill-mine text-on-fill ring-fill-mine",
    };
</script>

<span
    class={classes(
        "inline-flex items-center gap-6 rounded-input px-8 py-4 text-12 font-medium whitespace-nowrap ring-1 ring-inset",
        filled ? solid[tone] : quiet[tone],
        extra,
    )}
>
    {#if !filled && tone !== "neutral"}<span class="size-6 shrink-0 rounded-pill bg-current" aria-hidden="true"></span>{/if}
    {@render children()}
</span>
