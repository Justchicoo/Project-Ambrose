<!-- Project Ambrose by Imjustchico: The table body part of the table component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { cn, type WithElementRef } from "$lib/utils.js";
    import type { HTMLAttributes } from "svelte/elements";

    let {
        ref = $bindable(null),
        class: className,
        children,
        ...restProps
    }: WithElementRef<HTMLAttributes<HTMLTableSectionElement>> = $props();
</script>

<tbody bind:this={ref} data-slot="table-body" class={cn("[&_tr:last-child]:border-0", className)} {...restProps}>
    {@render children?.()}
</tbody>
