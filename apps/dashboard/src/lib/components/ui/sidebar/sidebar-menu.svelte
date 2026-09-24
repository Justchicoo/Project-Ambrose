<!-- Project Ambrose by Imjustchico: The sidebar menu part of the sidebar component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { cn, type WithElementRef } from "$lib/utils.js";
    import type { HTMLAttributes } from "svelte/elements";

    let {
        ref = $bindable(null),
        class: className,
        children,
        ...restProps
    }: WithElementRef<HTMLAttributes<HTMLUListElement>, HTMLUListElement> = $props();
</script>

<ul bind:this={ref} data-slot="sidebar-menu" data-sidebar="menu" class={cn("gap-0 flex w-full min-w-0 flex-col", className)} {...restProps}>
    {@render children?.()}
</ul>
