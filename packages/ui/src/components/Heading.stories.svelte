<!-- Project Ambrose by Imjustchico: A heading at each level of the display scale, in the order a page uses them. -->
<script module lang="ts">
    import { defineMeta } from "@storybook/addon-svelte-csf";
    import Heading from "./Heading.svelte";

    const { Story } = defineMeta({
        title: "Foundations/Heading",
        component: Heading,
        tags: ["autodocs"],
    });
</script>

<Story name="The scale">
    {#snippet template()}
        <div class="flex flex-col gap-12">
            <Heading level={1}>Wizard City</Heading>
            <Heading level={2}>Realms</Heading>
            <Heading level={3}>Ravenwood</Heading>
            <Heading level={4}>Players online</Heading>
        </div>
    {/snippet}
</Story>

<Story name="Smaller than its level">
    {#snippet template()}
        <Heading level={2} size="17">A card title that stays quiet</Heading>
    {/snippet}
</Story>
