<!-- Project Ambrose by Imjustchico: The card footer part of the card component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { cn, type WithElementRef } from "$lib/utils.js";
    import type { HTMLAttributes } from "svelte/elements";

    let { ref = $bindable(null), class: className, children, ...restProps }: WithElementRef<HTMLAttributes<HTMLDivElement>> = $props();
</script>

<div
    bind:this={ref}
    data-slot="card-footer"
    class={cn("bg-muted/50 rounded-b-xl border-t p-(--card-spacing) flex items-center", className)}
    {...restProps}
>
    {@render children?.()}
</div>
