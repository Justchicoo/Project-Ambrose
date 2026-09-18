<!-- Project Ambrose by Imjustchico: The label on its own and tied to a control. -->
<script module lang="ts">
    import { defineMeta } from "@storybook/addon-svelte-csf";
    import Label from "./Label.svelte";

    const { Story } = defineMeta({
        title: "Foundations/Label",
        component: Label,
        tags: ["autodocs"],
    });
</script>

<Story name="Naming a group">
    {#snippet template()}
        <Label>Players online</Label>
    {/snippet}
</Story>

<Story name="Naming a control">
    {#snippet template()}
        <div class="flex flex-col gap-6">
            <Label for="story-label-input">Realm name</Label>
            <input
                id="story-label-input"
                class="min-h-44 rounded-input border border-edge-strong bg-surface-sunken px-12 text-15 text-fg-body"
                value="Ambrose"
            />
        </div>
    {/snippet}
</Story>
