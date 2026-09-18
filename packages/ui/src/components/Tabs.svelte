<!-- Project Ambrose by Imjustchico: Tabs over one region, with arrow-key movement from the primitive layer and the chosen tab marked by a gold edge rather than by colour alone. -->
<script lang="ts">
    import type { Snippet } from "svelte";
    import { Tabs } from "bits-ui";
    import { classes } from "../internal/classes";

    type Panel = { value: string; label: string };

    type Props = {
        items: Panel[];
        value?: string;
        label: string;
        class?: string;
        panel: Snippet<[string]>;
    };

    let { items, value = $bindable(items[0]?.value ?? ""), label, class: extra, panel }: Props = $props();
</script>

<Tabs.Root bind:value class={classes("flex flex-col gap-16", extra)}>
    <Tabs.List aria-label={label} class="flex items-center gap-4 border-b border-edge-quiet">
        {#each items as item (item.value)}
            <Tabs.Trigger
                value={item.value}
                class="ambrose-hover min-h-44 border-b-2 border-transparent px-14 text-15 text-fg-muted data-[state=active]:border-action data-[state=active]:text-fg-body"
            >
                {item.label}
            </Tabs.Trigger>
        {/each}
    </Tabs.List>
    {#each items as item (item.value)}
        <Tabs.Content value={item.value}>
            {@render panel(item.value)}
        </Tabs.Content>
    {/each}
</Tabs.Root>
