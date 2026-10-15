<!-- Project Ambrose by Imjustchico: The record link by itself. -->
<script module lang="ts">
    import { defineMeta } from "@storybook/addon-svelte-csf";
    import RecordLink from "./RecordLink.svelte";

    const { Story } = defineMeta({
        title: "Data/RecordLink",
        component: RecordLink,
        tags: ["autodocs"],
    });
</script>

<Story name="Default">
    {#snippet template()}
        <RecordLink href="/types/WizardPlayer" label="WizardPlayer" title="Open the WizardPlayer class" />
    {/snippet}
</Story>
