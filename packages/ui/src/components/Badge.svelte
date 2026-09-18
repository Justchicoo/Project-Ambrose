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
        neutral: "border-edge-strong text-fg-muted",
        healthy: "border-state-healthy text-state-healthy",
        waiting: "border-state-waiting text-state-waiting",
        wrong: "border-state-wrong text-state-wrong",
        mine: "border-mine text-mine",
    };
    const solid: Record<Tone, string> = {
        neutral: "bg-surface-sunken text-fg-body border-edge-strong",
        healthy: "bg-fill-healthy text-on-fill border-fill-healthy",
        waiting: "bg-fill-action text-on-fill border-fill-action",
        wrong: "bg-fill-danger text-on-fill border-fill-danger",
        mine: "bg-fill-mine text-on-fill border-fill-mine",
    };
</script>

<span
    class={classes(
        "inline-flex items-center rounded-pill border px-10 py-4 text-11 font-medium",
        filled ? solid[tone] : quiet[tone],
        extra,
    )}
>
    {@render children()}
</span>
