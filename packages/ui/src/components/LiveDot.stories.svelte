<!-- Project Ambrose by Imjustchico: The live dot in both forms, and the proof that it still says Live with the motion turned off. -->
<script module lang="ts">
    import { defineMeta } from "@storybook/addon-svelte-csf";
    import { expect } from "storybook/test";
    import LiveDot from "./LiveDot.svelte";

    const { Story } = defineMeta({
        title: "Components/LiveDot",
        component: LiveDot,
    });
</script>

<Story name="Live">
    {#snippet template()}
        <LiveDot age="2s ago" />
    {/snippet}
</Story>

<Story name="Stale">
    {#snippet template()}
        <LiveDot age="4m ago" stale />
    {/snippet}
</Story>

<Story
    name="Still live with the motion off"
    play={async ({ canvas }) => {
        const root = document.documentElement;
        const before = root.getAttribute("data-motion");
        root.setAttribute("data-motion", "off");
        try {
            const dot = canvas.getByTestId("live-dot");
            await expect(getComputedStyle(dot).animationDuration).toBe("0s");
            await expect(canvas.getByText("Live")).toBeInTheDocument();
            await expect(canvas.getByText("9s ago")).toBeInTheDocument();
        } finally {
            if (before === null) {
                root.removeAttribute("data-motion");
            } else {
                root.setAttribute("data-motion", before);
            }
        }
    }}
>
    {#snippet template()}
        <LiveDot age="9s ago" />
    {/snippet}
</Story>
