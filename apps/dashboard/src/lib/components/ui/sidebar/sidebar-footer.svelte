<!-- Project Ambrose by Imjustchico: The sidebar footer part of the sidebar component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { cn, type WithElementRef } from "$lib/utils.js";
    import type { HTMLAttributes } from "svelte/elements";

    let { ref = $bindable(null), class: className, children, ...restProps }: WithElementRef<HTMLAttributes<HTMLElement>> = $props();
</script>

<div bind:this={ref} data-slot="sidebar-footer" data-sidebar="footer" class={cn("gap-2 p-2 flex flex-col", className)} {...restProps}>
    {@render children?.()}
</div>
