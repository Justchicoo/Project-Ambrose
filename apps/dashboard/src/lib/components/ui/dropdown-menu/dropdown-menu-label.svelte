<!-- Project Ambrose by Imjustchico: The dropdown menu label part of the dropdown menu component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { cn, type WithElementRef } from "$lib/utils.js";
    import type { HTMLAttributes } from "svelte/elements";

    let {
        ref = $bindable(null),
        class: className,
        inset,
        children,
        ...restProps
    }: WithElementRef<HTMLAttributes<HTMLDivElement>> & {
        inset?: boolean;
    } = $props();
</script>

<div
    bind:this={ref}
    data-slot="dropdown-menu-label"
    data-inset={inset}
    class={cn("text-muted-foreground px-1.5 py-1 text-xs font-medium data-inset:pl-7", className)}
    {...restProps}
>
    {@render children?.()}
</div>
