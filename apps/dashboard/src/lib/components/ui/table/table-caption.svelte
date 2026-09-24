<!-- Project Ambrose by Imjustchico: The table caption part of the table component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { cn, type WithElementRef } from "$lib/utils.js";
    import type { HTMLAttributes } from "svelte/elements";

    let { ref = $bindable(null), class: className, children, ...restProps }: WithElementRef<HTMLAttributes<HTMLElement>> = $props();
</script>

<caption bind:this={ref} data-slot="table-caption" class={cn("text-muted-foreground mt-4 text-sm", className)} {...restProps}>
    {@render children?.()}
</caption>
