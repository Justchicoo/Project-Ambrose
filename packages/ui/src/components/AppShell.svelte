<!-- Project Ambrose by Imjustchico: The frame both surfaces share: a chrome bar of the fixed height, an optional side bar of the fixed width, and one main region with a skip link to it. A page that fills the frame edge to edge, as the launcher window does, asks to bleed and is given the region whole, with its own regions deciding where the padding and the scrolling go. -->
<script lang="ts">
    import type { Snippet } from "svelte";
    import { classes } from "../internal/classes";

    type Props = {
        product: string;
        skipLabel?: string;
        bleed?: boolean;
        class?: string;
        side?: Snippet;
        barStart?: Snippet;
        barEnd?: Snippet;
        children: Snippet;
    };

    let { product, skipLabel = "Skip to content", bleed = false, class: extra, side, barStart, barEnd, children }: Props = $props();
</script>

<div class={classes("flex h-full min-h-screen flex-col bg-surface-page text-fg-body", extra)}>
    <a
        href="#ambrose-main"
        class="ambrose-hover absolute left-8 top-8 z-50 -translate-y-44 rounded-input bg-surface-card px-12 py-10 text-13 focus:translate-y-0"
    >
        {skipLabel}
    </a>
    <header
        style="height: var(--ambrose-size-chrome-bar)"
        class="flex shrink-0 items-center justify-between gap-16 border-b border-edge-quiet bg-surface-chrome px-16"
    >
        <div class="flex items-center gap-12">
            <span class="font-display text-17 font-semibold text-fg-body">{product}</span>
            {#if barStart}{@render barStart()}{/if}
        </div>
        {#if barEnd}
            <div class="flex items-center gap-8">{@render barEnd()}</div>
        {/if}
    </header>
    <div class="flex min-h-0 flex-1">
        {#if side}
            <div style="width: var(--ambrose-size-side-bar)" class="hidden shrink-0 border-r border-edge-quiet bg-surface-chrome md:block">
                {@render side()}
            </div>
        {/if}
        <main
            id="ambrose-main"
            class={classes("min-w-0 flex-1", bleed ? "flex min-h-0 flex-col overflow-hidden" : "overflow-auto p-20")}
            data-ambrose-scroll={bleed ? undefined : true}
        >
            {@render children()}
        </main>
    </div>
</div>
