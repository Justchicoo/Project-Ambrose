<!-- Project Ambrose by Imjustchico: The table cell part of the table component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { cn, type WithElementRef } from "$lib/utils.js";
    import type { HTMLTdAttributes } from "svelte/elements";

    let { ref = $bindable(null), class: className, children, ...restProps }: WithElementRef<HTMLTdAttributes> = $props();
</script>

<td
    bind:this={ref}
    data-slot="table-cell"
    class={cn("p-2 align-middle whitespace-nowrap [&:has([role=checkbox])]:pr-0", className)}
    {...restProps}
>
    {@render children?.()}
</td>
