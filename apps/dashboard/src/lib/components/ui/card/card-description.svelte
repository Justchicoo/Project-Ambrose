<!-- Project Ambrose by Imjustchico: The card description part of the card component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { cn, type WithElementRef } from "$lib/utils.js";
    import type { HTMLAttributes } from "svelte/elements";

    let {
        ref = $bindable(null),
        class: className,
        children,
        ...restProps
    }: WithElementRef<HTMLAttributes<HTMLParagraphElement>> = $props();
</script>

<p bind:this={ref} data-slot="card-description" class={cn("text-muted-foreground text-sm", className)} {...restProps}>
    {@render children?.()}
</p>
