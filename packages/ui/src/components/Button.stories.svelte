<!-- Project Ambrose by Imjustchico: The button in every variant and every state, including the one gold action a screen is allowed. -->
<script module lang="ts">
    import { defineMeta } from "@storybook/addon-svelte-csf";
    import Button from "./Button.svelte";

    const { Story } = defineMeta({
        title: "Controls/Button",
        component: Button,
        tags: ["autodocs"],
    });
</script>

<Story name="The one action">
    {#snippet template()}
        <div class="flex items-center gap-12">
            <Button variant="action" size="wide" icon="play">Play</Button>
            <Button variant="action">Start server</Button>
        </div>
    {/snippet}
</Story>

<Story name="Quiet and ghost">
    {#snippet template()}
        <div class="flex items-center gap-12">
            <Button variant="quiet">Cancel</Button>
            <Button variant="quiet" icon="refresh-cw">Reload</Button>
            <Button variant="ghost">Details</Button>
        </div>
    {/snippet}
</Story>

<Story name="Destructive">
    {#snippet template()}
        <Button variant="danger" icon="trash-2">Delete backup</Button>
    {/snippet}
</Story>

<Story name="Disabled and busy">
    {#snippet template()}
        <div class="flex items-center gap-12">
            <Button variant="action" disabled>Start server</Button>
            <Button variant="quiet" disabled>Cancel</Button>
            <Button variant="action" busy busyLabel="Starting">Start server</Button>
        </div>
    {/snippet}
</Story>

<Story name="As a link">
    {#snippet template()}
        <Button href="#audit" variant="quiet" icon="arrow-right">Open the audit log</Button>
    {/snippet}
</Story>
