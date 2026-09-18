<!-- Project Ambrose by Imjustchico: The single choice, closed, chosen and disabled. -->
<script module lang="ts">
    import { defineMeta } from "@storybook/addon-svelte-csf";
    import Select from "./Select.svelte";

    const { Story } = defineMeta({
        title: "Controls/Select",
        component: Select,
        tags: ["autodocs"],
    });

    const realms = [
        { value: "ambrose", label: "Ambrose" },
        { value: "ravenwood", label: "Ravenwood" },
        { value: "krokotopia", label: "Krokotopia", disabled: true },
    ];
</script>

<Story name="Nothing chosen">
    {#snippet template()}
        <Select id="story-select-empty" label="Realm" items={realms} placeholder="Choose a realm" />
    {/snippet}
</Story>

<Story name="Chosen">
    {#snippet template()}
        <Select id="story-select-chosen" label="Realm" items={realms} value="ravenwood" hint="Where the command runs." />
    {/snippet}
</Story>

<Story name="Disabled">
    {#snippet template()}
        <Select id="story-select-disabled" label="Realm" items={realms} value="ambrose" disabled />
    {/snippet}
</Story>
