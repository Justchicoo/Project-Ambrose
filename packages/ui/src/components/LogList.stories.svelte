<!-- Project Ambrose by Imjustchico: Log records as rows, with every level and with ANSI in the message. -->
<script module lang="ts">
    import { defineMeta } from "@storybook/addon-svelte-csf";
    import LogList from "./LogList.svelte";

    const { Story } = defineMeta({
        title: "Data/LogList",
        component: LogList,
        tags: ["autodocs"],
    });

    const levels = ["trace", "debug", "info", "warn", "error", "fatal"] as const;

    const records = Array.from({ length: 200 }, (_value, index) => ({
        sequence: index + 1,
        time: `12:0${index % 10}:11`,
        level: levels[index % levels.length],
        category: "world",
        message: index % 7 === 0 ? "[31mzone refused[0m: wizardcity/ravenwood" : `loaded zone ${index} in 4 ms`,
    }));
</script>

<Story name="Two hundred records">
    {#snippet template()}
        <LogList label="Game server console" {records} />
    {/snippet}
</Story>

<Story name="Nothing yet">
    {#snippet template()}
        <LogList label="Game server console" records={[]} height="8rem" />
    {/snippet}
</Story>
