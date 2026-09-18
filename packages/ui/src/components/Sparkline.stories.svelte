<!-- Project Ambrose by Imjustchico: One series drawn small, including the gap a stopped app leaves behind. -->
<script module lang="ts">
    import { defineMeta } from "@storybook/addon-svelte-csf";
    import Sparkline from "./Sparkline.svelte";

    const { Story } = defineMeta({
        title: "Data/Sparkline",
        component: Sparkline,
        tags: ["autodocs"],
    });

    const times = Array.from({ length: 60 }, (_value, index) => index);
    const steady = times.map((index) => 50 + Math.round(20 * Math.sin(index / 5)));
    const withGap = steady.map((value, index) => (index > 20 && index < 32 ? null : value));
</script>

<Story name="A steady series">
    {#snippet template()}
        <div class="w-full rounded-card bg-surface-card p-16">
            <Sparkline label="Players online over the last hour" {times} values={steady} unit="Players" />
        </div>
    {/snippet}
</Story>

<Story name="A stretch with no readings">
    {#snippet template()}
        <div class="w-full rounded-card bg-surface-card p-16">
            <Sparkline label="Players online while the server was stopped" {times} values={withGap} unit="Players" />
        </div>
    {/snippet}
</Story>
