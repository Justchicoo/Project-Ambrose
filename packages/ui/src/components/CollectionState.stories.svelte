<!-- Project Ambrose by Imjustchico: The four states a collection shows instead of its rows, each as its own story. -->
<script module lang="ts">
    import { defineMeta } from "@storybook/addon-svelte-csf";
    import CollectionState from "./CollectionState.svelte";

    const { Story } = defineMeta({
        title: "Components/CollectionState",
        component: CollectionState,
    });
</script>

<Story name="Loading" tags={["state:loading"]}>
    {#snippet template()}
        <CollectionState status="loading" label="the sessions" />
    {/snippet}
</Story>

<Story name="Empty" tags={["state:empty"]}>
    {#snippet template()}
        <CollectionState status="empty" label="the sessions" empty="No session has been opened yet" />
    {/snippet}
</Story>

<Story name="Nothing matched the filter" tags={["state:no-results"]}>
    {#snippet template()}
        <CollectionState status="no-results" label="the sessions" noResults="No session matches that filter" />
    {/snippet}
</Story>

<Story name="Could not be loaded" tags={["state:error"]}>
    {#snippet template()}
        <CollectionState status="error" label="the sessions" error="The sessions could not be loaded" />
    {/snippet}
</Story>
