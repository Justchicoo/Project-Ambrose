<!-- Project Ambrose by Imjustchico: A heading in the display family, at one of the sizes the type scale allows, rendered as the real heading level a screen reader needs. The two largest belong to a surface with one thing to say, such as the launcher window, and are asked for by name rather than reached by a level. -->
<script lang="ts">
    import type { Snippet } from "svelte";
    import { classes } from "../internal/classes";

    type Props = {
        level?: 1 | 2 | 3 | 4;
        size?: "56" | "44" | "34" | "26" | "21" | "17";
        id?: string;
        class?: string;
        children: Snippet;
    };

    let { level = 2, size, id, class: extra, children }: Props = $props();

    const defaults: Record<number, "56" | "44" | "34" | "26" | "21" | "17"> = { 1: "34", 2: "26", 3: "21", 4: "17" };
    const step = $derived(size ?? defaults[level]);
    const steps: Record<string, string> = {
        "56": "text-56",
        "44": "text-44",
        "34": "text-34",
        "26": "text-26",
        "21": "text-21",
        "17": "text-17",
    };
    const sizeClass = $derived(steps[step]);
</script>

<svelte:element this={`h${level}`} {id} class={classes("font-display font-semibold text-fg-body", sizeClass, extra)}>
    {@render children()}
</svelte:element>
