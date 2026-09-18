<!-- Project Ambrose by Imjustchico: Monospaced, tabular text for a count, a timing, a revision, a path or a command, so numbers line up down a column. -->
<script lang="ts">
    import type { Snippet } from "svelte";
    import { classes } from "../internal/classes";

    type Props = {
        size?: "11" | "12" | "13" | "15" | "17" | "21" | "26";
        tone?: "body" | "muted" | "faint";
        title?: string;
        class?: string;
        children: Snippet;
    };

    let { size = "13", tone = "body", title, class: extra, children }: Props = $props();

    const sizes: Record<string, string> = {
        "11": "text-11",
        "12": "text-12",
        "13": "text-13",
        "15": "text-15",
        "17": "text-17",
        "21": "text-21",
        "26": "text-26",
    };
    const tones: Record<string, string> = {
        body: "text-fg-body",
        muted: "text-fg-muted",
        faint: "text-fg-faint",
    };
</script>

<span {title} class={classes("ambrose-mono", sizes[size], tones[tone], extra)}>{@render children()}</span>
