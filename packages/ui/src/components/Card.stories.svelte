<!-- Project Ambrose by Imjustchico: The card plain, with its own actions and with a footer. -->
<script module lang="ts">
    import { defineMeta } from "@storybook/addon-svelte-csf";
    import Card from "./Card.svelte";
    import Button from "./Button.svelte";
    import Mono from "./Mono.svelte";

    const { Story } = defineMeta({
        title: "Containers/Card",
        component: Card,
        tags: ["autodocs"],
    });
</script>

<Story name="Plain">
    {#snippet template()}
        <Card title="Ravenwood" id="story-card-plain">
            <p class="text-13 text-fg-muted">One realm, started at boot and watched by the supervisor.</p>
        </Card>
    {/snippet}
</Story>

<Story name="With actions and a footer">
    {#snippet template()}
        <Card title="Backups" id="story-card-actions">
            {#snippet actions()}
                <Button variant="quiet" icon="download">Download</Button>
            {/snippet}
            <p class="text-13 text-fg-muted">The last archive was verified against its manifest.</p>
            <Mono tone="muted">ambrose-2026-09-18-0400.tar.zst</Mono>
            {#snippet footer()}
                Kept for thirty days
            {/snippet}
        </Card>
    {/snippet}
</Story>

<Story name="Untitled">
    {#snippet template()}
        <Card>
            <p class="text-13 text-fg-body">A card can hold anything, including nothing worth a title.</p>
        </Card>
    {/snippet}
</Story>
