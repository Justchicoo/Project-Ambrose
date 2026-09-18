<!-- Project Ambrose by Imjustchico: The switch on, off and disabled, always saying which it is in words. -->
<script module lang="ts">
    import { defineMeta } from "@storybook/addon-svelte-csf";
    import Switch from "./Switch.svelte";

    const { Story } = defineMeta({
        title: "Controls/Switch",
        component: Switch,
        tags: ["autodocs"],
    });
</script>

<Story name="On and off">
    {#snippet template()}
        <div class="flex w-full flex-col gap-8">
            <Switch id="story-switch-on" label="Maintenance mode" checked={true} />
            <Switch id="story-switch-off" label="Plain HTTP beyond this machine" />
            <Switch id="story-switch-locked" label="Reduced motion" disabled onWord="Always" offWord="Follow the system" />
        </div>
    {/snippet}
</Story>
