<!-- Project Ambrose by Imjustchico: The skeleton component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { cn, type WithElementRef, type WithoutChildren } from "$lib/utils.js";
    import type { HTMLAttributes } from "svelte/elements";

    let {
        ref = $bindable(null),
        class: className,
        ...restProps
    }: WithoutChildren<WithElementRef<HTMLAttributes<HTMLDivElement>>> = $props();
</script>

<div bind:this={ref} data-slot="skeleton" class={cn("bg-muted rounded-md animate-pulse", className)} {...restProps}></div>
