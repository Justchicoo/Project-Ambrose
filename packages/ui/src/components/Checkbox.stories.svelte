<!-- Project Ambrose by Imjustchico: The checkbox unchecked, checked, part-way and disabled. -->
<script module lang="ts">
    import { defineMeta } from "@storybook/addon-svelte-csf";
    import Checkbox from "./Checkbox.svelte";

    const { Story } = defineMeta({
        title: "Controls/Checkbox",
        component: Checkbox,
        tags: ["autodocs"],
    });
</script>

<Story name="Every state">
    {#snippet template()}
        <div class="flex flex-col gap-12">
            <Checkbox id="story-check-off" label="Restart the app when it exits" />
            <Checkbox id="story-check-on" label="Keep a backup before every update" checked={true} hint="Kept for thirty days." />
            <Checkbox id="story-check-part" label="Every realm" indeterminate />
            <Checkbox id="story-check-locked" label="Serve executables from the patch server" disabled />
        </div>
    {/snippet}
</Story>
