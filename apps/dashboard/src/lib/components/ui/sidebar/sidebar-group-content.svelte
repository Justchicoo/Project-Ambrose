<!-- Project Ambrose by Imjustchico: The sidebar group content part of the sidebar component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { cn, type WithElementRef } from "$lib/utils.js";
    import type { HTMLAttributes } from "svelte/elements";

    let { ref = $bindable(null), class: className, children, ...restProps }: WithElementRef<HTMLAttributes<HTMLDivElement>> = $props();
</script>

<div bind:this={ref} data-slot="sidebar-group-content" data-sidebar="group-content" class={cn("text-sm w-full", className)} {...restProps}>
    {@render children?.()}
</div>
