<!-- Project Ambrose by Imjustchico: One number worth watching, alone and with its sparkline. -->
<script module lang="ts">
    import { defineMeta } from "@storybook/addon-svelte-csf";
    import StatTile from "./StatTile.svelte";
    import Sparkline from "./Sparkline.svelte";

    const { Story } = defineMeta({
        title: "Data/StatTile",
        component: StatTile,
        tags: ["autodocs"],
    });

    const times = Array.from({ length: 30 }, (_value, index) => index);
    const values = times.map((index) => (index === 12 ? null : 40 + Math.round(18 * Math.sin(index / 3))));
</script>

<Story name="A row of tiles">
    {#snippet template()}
        <div class="grid grid-cols-3 gap-16">
            <StatTile label="Players online" value="128" state="healthy" word="Healthy" />
            <StatTile label="Memory" value="3.4" unit="GB" state="waiting" word="Near the limit" />
            <StatTile label="Last backup" value="04:00" state="unknown" word="Not verified" />
        </div>
    {/snippet}
</Story>

<Story name="With a sparkline">
    {#snippet template()}
        <StatTile label="Players online" value="128" state="healthy" word="Healthy">
            {#snippet chart()}
                <Sparkline label="Players online over the last fifteen minutes" {times} {values} unit="Players" />
            {/snippet}
        </StatTile>
    {/snippet}
</Story>
