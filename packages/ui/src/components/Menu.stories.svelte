<!-- Project Ambrose by Imjustchico: The extra actions behind one trigger, with the destructive one marked. -->
<script module lang="ts">
    import { defineMeta } from "@storybook/addon-svelte-csf";
    import Menu from "./Menu.svelte";

    const { Story } = defineMeta({
        title: "Overlays/Menu",
        component: Menu,
        tags: ["autodocs"],
    });

    const entries = [
        { id: "restart", label: "Restart", icon: "refresh-cw" as const },
        { id: "backup", label: "Back up now", icon: "archive" as const },
        { id: "logs", label: "Download logs", icon: "download" as const, disabled: true },
        { id: "delete", label: "Delete server", icon: "trash-2" as const, danger: true },
    ];
</script>

<Story name="Row actions">
    {#snippet template()}
        <Menu label="More actions for Ravenwood" items={entries} />
    {/snippet}
</Story>
