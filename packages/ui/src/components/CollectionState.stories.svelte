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
    <CollectionState status="loading" label="the sessions" />
</Story>

<Story name="Empty" tags={["state:empty"]}>
    <CollectionState status="empty" label="the sessions" empty="No session has been opened yet" />
</Story>

<Story name="Nothing matched the filter" tags={["state:no-results"]}>
    <CollectionState status="no-results" label="the sessions" noResults="No session matches that filter" />
</Story>

<Story name="Could not be loaded" tags={["state:error"]}>
    <CollectionState status="error" label="the sessions" error="The sessions could not be loaded" />
</Story>
