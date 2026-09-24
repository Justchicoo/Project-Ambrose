<!-- Project Ambrose by Imjustchico: The sidebar menu sub item part of the sidebar component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { cn, type WithElementRef } from "$lib/utils.js";
    import type { HTMLAttributes } from "svelte/elements";

    let { ref = $bindable(null), children, class: className, ...restProps }: WithElementRef<HTMLAttributes<HTMLLIElement>> = $props();
</script>

<li
    bind:this={ref}
    data-slot="sidebar-menu-sub-item"
    data-sidebar="menu-sub-item"
    class={cn("group/menu-sub-item relative", className)}
    {...restProps}
>
    {@render children?.()}
</li>
