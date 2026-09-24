<!-- Project Ambrose by Imjustchico: A hint beside a control, never the only place a meaning is written. -->
<script module lang="ts">
    import { defineMeta } from "@storybook/addon-svelte-csf";
    import Tooltip from "./Tooltip.svelte";
    import IconButton from "./IconButton.svelte";

    const { Story } = defineMeta({
        title: "Overlays/Tooltip",
        component: Tooltip,
        tags: ["autodocs"],
    });
</script>

<Story name="On a control">
    {#snippet template()}
        <Tooltip text="Reloads every manager from the world database">
            {#snippet children(props)}
                <IconButton {...props} icon="refresh-cw" label="Reload the world data" variant="quiet" />
            {/snippet}
        </Tooltip>
    {/snippet}
</Story>
