<!-- Project Ambrose by Imjustchico: The confirmation a destructive action has to pass, naming the thing it will destroy. -->
<script module lang="ts">
    import { defineMeta } from "@storybook/addon-svelte-csf";
    import ConfirmDialog from "./ConfirmDialog.svelte";

    const { Story } = defineMeta({
        title: "Overlays/ConfirmDialog",
        component: ConfirmDialog,
        tags: ["autodocs"],
    });
</script>

<Story name="Open">
    {#snippet template()}
        <ConfirmDialog
            open
            title="Delete this backup"
            description="The archive and its manifest are removed from this machine. Nothing else is touched."
            subject="ambrose-2026-09-18-0400.tar.zst"
            confirmLabel="Delete backup"
        />
    {/snippet}
</Story>
