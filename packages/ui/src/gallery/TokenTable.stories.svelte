<!-- Project Ambrose by Imjustchico: The tokens page in both themes, which is where a contrast figure is read rather than guessed. -->
<script module lang="ts">
    import { defineMeta } from "@storybook/addon-svelte-csf";
    import TokenTable from "./TokenTable.svelte";

    const { Story } = defineMeta({
        title: "Foundations/Tokens",
        component: TokenTable,
        tags: ["autodocs"],
    });
</script>

<Story name="Dark">
    {#snippet template()}
        <TokenTable theme="dark" />
    {/snippet}
</Story>

<Story name="Light">
    {#snippet template()}
        <TokenTable theme="light" />
    {/snippet}
</Story>
