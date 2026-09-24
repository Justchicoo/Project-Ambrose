<!-- Project Ambrose by Imjustchico: The icon-only control in each variant, each carrying the label it is required to have. -->
<script module lang="ts">
    import { defineMeta } from "@storybook/addon-svelte-csf";
    import IconButton from "./IconButton.svelte";

    const { Story } = defineMeta({
        title: "Controls/IconButton",
        component: IconButton,
        tags: ["autodocs"],
    });
</script>

<Story name="The variants">
    {#snippet template()}
        <div class="flex items-center gap-12">
            <IconButton icon="refresh-cw" label="Reload the page" />
            <IconButton icon="copy" label="Copy the path" variant="quiet" />
            <IconButton icon="trash-2" label="Delete this backup" variant="danger" />
        </div>
    {/snippet}
</Story>

<Story name="Pressed and disabled">
    {#snippet template()}
        <div class="flex items-center gap-12">
            <IconButton icon="pause" label="Pause the console" variant="quiet" pressed={true} />
            <IconButton icon="power" label="Stop the server" variant="quiet" disabled />
        </div>
    {/snippet}
</Story>
