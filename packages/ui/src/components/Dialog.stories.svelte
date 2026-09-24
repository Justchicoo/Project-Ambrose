<!-- Project Ambrose by Imjustchico: The dialog closed behind its trigger and open with its footer. -->
<script module lang="ts">
    import { defineMeta } from "@storybook/addon-svelte-csf";
    import Dialog from "./Dialog.svelte";
    import Button from "./Button.svelte";
    import TextField from "./TextField.svelte";

    const { Story } = defineMeta({
        title: "Overlays/Dialog",
        component: Dialog,
        tags: ["autodocs"],
    });
</script>

<Story name="Behind a trigger">
    {#snippet template()}
        <Dialog title="New realm" description="A realm is one world shard players can pick.">
            {#snippet trigger(props)}
                <Button {...props} variant="quiet" icon="plus">New realm</Button>
            {/snippet}
            <TextField id="story-dialog-name" label="Realm name" value="Ambrose" />
            {#snippet footer()}
                <Button variant="quiet">Cancel</Button>
                <Button variant="action">Create</Button>
            {/snippet}
        </Dialog>
    {/snippet}
</Story>

<Story name="Open">
    {#snippet template()}
        <Dialog open title="Rename the realm" description="Players see the new name the next time they sign in.">
            <TextField id="story-dialog-rename" label="Realm name" value="Ravenwood" />
            {#snippet footer()}
                <Button variant="quiet">Cancel</Button>
                <Button variant="action">Save</Button>
            {/snippet}
        </Dialog>
    {/snippet}
</Story>
