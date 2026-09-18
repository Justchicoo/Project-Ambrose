<!-- Project Ambrose by Imjustchico: What a region says when it holds nothing, with and without the one thing to do about it. -->
<script module lang="ts">
    import { defineMeta } from "@storybook/addon-svelte-csf";
    import EmptyState from "./EmptyState.svelte";
    import Button from "./Button.svelte";

    const { Story } = defineMeta({
        title: "State/EmptyState",
        component: EmptyState,
        tags: ["autodocs"],
    });
</script>

<Story name="With something to do">
    {#snippet template()}
        <EmptyState icon="archive" title="No backups yet" detail="A backup holds every database with the config and data folders.">
            {#snippet action()}
                <Button variant="action" icon="plus">Back up now</Button>
            {/snippet}
        </EmptyState>
    {/snippet}
</Story>

<Story name="Nothing to do">
    {#snippet template()}
        <EmptyState icon="users" title="Nobody is online" detail="Players appear here the moment one signs in." />
    {/snippet}
</Story>
