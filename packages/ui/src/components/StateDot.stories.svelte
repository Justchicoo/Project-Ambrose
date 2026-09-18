<!-- Project Ambrose by Imjustchico: The four states, each with its word, and the one dot that is allowed to keep moving. -->
<script module lang="ts">
    import { defineMeta } from "@storybook/addon-svelte-csf";
    import StateDot from "./StateDot.svelte";

    const { Story } = defineMeta({
        title: "State/StateDot",
        component: StateDot,
        tags: ["autodocs"],
    });
</script>

<Story name="The four states">
    {#snippet template()}
        <div class="flex flex-col gap-8">
            <StateDot state="healthy" word="Running" />
            <StateDot state="waiting" word="Starting" />
            <StateDot state="wrong" word="Crashed" />
            <StateDot state="unknown" word="Not reported" />
        </div>
    {/snippet}
</Story>

<Story name="A live connection">
    {#snippet template()}
        <StateDot state="healthy" word="Connected" live />
    {/snippet}
</Story>
