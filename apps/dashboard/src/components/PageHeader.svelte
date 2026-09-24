<!-- Project Ambrose by Imjustchico: The title block every page opens with: the page's name, one line on what it is for, and the page's own buttons on the right. -->
<script lang="ts">
    import type { Snippet } from "svelte";

    type Props = { title: string; description: string; actions?: Snippet };
    let { title, description, actions }: Props = $props();
</script>

<div class="flex flex-col gap-4 sm:flex-row sm:items-end sm:justify-between">
    <div class="space-y-1">
        <h1 class="font-serif text-3xl font-semibold tracking-tight">{title}</h1>
        <p class="text-sm text-muted-foreground">{description}</p>
    </div>
    {#if actions}
        <div class="flex flex-wrap gap-2">{@render actions()}</div>
    {/if}
</div>
