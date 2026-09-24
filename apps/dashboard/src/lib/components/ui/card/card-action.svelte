<!-- Project Ambrose by Imjustchico: The card action part of the card component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { cn, type WithElementRef } from "$lib/utils.js";
    import type { HTMLAttributes } from "svelte/elements";

    let { ref = $bindable(null), class: className, children, ...restProps }: WithElementRef<HTMLAttributes<HTMLDivElement>> = $props();
</script>

<div
    bind:this={ref}
    data-slot="card-action"
    class={cn("col-start-2 row-span-2 row-start-1 self-start justify-self-end", className)}
    {...restProps}
>
    {@render children?.()}
</div>
