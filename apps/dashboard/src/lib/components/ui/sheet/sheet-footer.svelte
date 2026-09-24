<!-- Project Ambrose by Imjustchico: The sheet footer part of the sheet component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { cn, type WithElementRef } from "$lib/utils.js";
    import type { HTMLAttributes } from "svelte/elements";

    let { ref = $bindable(null), class: className, children, ...restProps }: WithElementRef<HTMLAttributes<HTMLDivElement>> = $props();
</script>

<div bind:this={ref} data-slot="sheet-footer" class={cn("gap-2 p-4 mt-auto flex flex-col", className)} {...restProps}>
    {@render children?.()}
</div>
