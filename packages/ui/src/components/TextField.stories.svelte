<!-- Project Ambrose by Imjustchico: The text input with its hint, its error and its monospaced form. -->
<script module lang="ts">
    import { defineMeta } from "@storybook/addon-svelte-csf";
    import TextField from "./TextField.svelte";

    const { Story } = defineMeta({
        title: "Controls/TextField",
        component: TextField,
        tags: ["autodocs"],
    });
</script>

<Story name="With a hint">
    {#snippet template()}
        <TextField id="story-realm" label="Realm name" value="Ambrose" hint="Players see this on the realm list." />
    {/snippet}
</Story>

<Story name="Wrong">
    {#snippet template()}
        <TextField id="story-port" label="Game server port" value="70000" error="A port is between 1 and 65535." />
    {/snippet}
</Story>

<Story name="A path">
    {#snippet template()}
        <TextField id="story-path" label="Install folder" value="C:/Games/Wizard101" mono readonly />
    {/snippet}
</Story>

<Story name="Disabled">
    {#snippet template()}
        <TextField id="story-locked" label="Revision" value="V_r806919" disabled hint="Set by the install in use." />
    {/snippet}
</Story>
