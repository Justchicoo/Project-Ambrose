<!-- Project Ambrose by Imjustchico: Tabs over one region, with the chosen tab marked by an edge as well as by colour. -->
<script module lang="ts">
    import { defineMeta } from "@storybook/addon-svelte-csf";
    import Tabs from "./Tabs.svelte";

    const { Story } = defineMeta({
        title: "Containers/Tabs",
        component: Tabs,
        tags: ["autodocs"],
    });

    const panels = [
        { value: "status", label: "Status" },
        { value: "console", label: "Console" },
        { value: "settings", label: "Settings" },
    ];
</script>

<Story name="Three panels">
    {#snippet template()}
        <Tabs items={panels} label="Server views">
            {#snippet panel(value)}
                <p class="text-13 text-fg-muted">The {value} panel for this server.</p>
            {/snippet}
        </Tabs>
    {/snippet}
</Story>

<Story name="Starting on the second">
    {#snippet template()}
        <Tabs items={panels} value="console" label="Server views">
            {#snippet panel(value)}
                <p class="text-13 text-fg-muted">The {value} panel for this server.</p>
            {/snippet}
        </Tabs>
    {/snippet}
</Story>
