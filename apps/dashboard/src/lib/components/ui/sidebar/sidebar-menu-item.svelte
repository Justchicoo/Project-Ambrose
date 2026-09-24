<!-- Project Ambrose by Imjustchico: The sidebar menu item part of the sidebar component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { cn, type WithElementRef } from "$lib/utils.js";
    import type { HTMLAttributes } from "svelte/elements";

    let {
        ref = $bindable(null),
        class: className,
        children,
        ...restProps
    }: WithElementRef<HTMLAttributes<HTMLLIElement>, HTMLLIElement> = $props();
</script>

<li bind:this={ref} data-slot="sidebar-menu-item" data-sidebar="menu-item" class={cn("group/menu-item relative", className)} {...restProps}>
    {@render children?.()}
</li>
