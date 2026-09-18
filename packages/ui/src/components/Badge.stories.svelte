<!-- Project Ambrose by Imjustchico: The badge in every tone, quiet and filled. -->
<script module lang="ts">
    import { defineMeta } from "@storybook/addon-svelte-csf";
    import Badge from "./Badge.svelte";

    const { Story } = defineMeta({
        title: "State/Badge",
        component: Badge,
        tags: ["autodocs"],
    });
</script>

<Story name="Quiet">
    {#snippet template()}
        <div class="flex flex-wrap items-center gap-8">
            <Badge>Stopped</Badge>
            <Badge tone="healthy">Verified</Badge>
            <Badge tone="waiting">Queued</Badge>
            <Badge tone="wrong">Failed</Badge>
            <Badge tone="mine">Your wizard</Badge>
        </div>
    {/snippet}
</Story>

<Story name="Filled">
    {#snippet template()}
        <div class="flex flex-wrap items-center gap-8">
            <Badge filled>Stopped</Badge>
            <Badge filled tone="healthy">Verified</Badge>
            <Badge filled tone="waiting">Queued</Badge>
            <Badge filled tone="wrong">Failed</Badge>
            <Badge filled tone="mine">Your wizard</Badge>
        </div>
    {/snippet}
</Story>
