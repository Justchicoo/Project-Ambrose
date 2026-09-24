<!-- Project Ambrose by Imjustchico: A raised area with an optional title, its own actions and a footer, which is what nearly every panel page is built out of. -->
<script lang="ts">
    import type { Snippet } from "svelte";
    import { classes } from "../internal/classes";
    import Heading from "./Heading.svelte";

    type Props = {
        title?: string;
        headingLevel?: 2 | 3 | 4;
        id?: string;
        class?: string;
        actions?: Snippet;
        footer?: Snippet;
        children: Snippet;
    };

    let { title, headingLevel = 3, id, class: extra, actions, footer, children }: Props = $props();

    const titleId = $derived(id ? `${id}-title` : undefined);
</script>

<section
    {id}
    aria-labelledby={title ? titleId : undefined}
    class={classes("flex flex-col rounded-card border border-edge-quiet bg-surface-card", extra)}
>
    {#if title || actions}
        <header class="flex min-h-44 items-center justify-between gap-16 border-b border-edge-quiet px-16 py-10">
            {#if title}
                <Heading level={headingLevel} size="17" id={titleId}>{title}</Heading>
            {:else}
                <span></span>
            {/if}
            {#if actions}
                <div class="flex items-center gap-8">{@render actions()}</div>
            {/if}
        </header>
    {/if}
    <div class="flex flex-col gap-12 p-16">
        {@render children()}
    </div>
    {#if footer}
        <footer class="border-t border-edge-quiet px-16 py-10 text-12 text-fg-faint">
            {@render footer()}
        </footer>
    {/if}
</section>
