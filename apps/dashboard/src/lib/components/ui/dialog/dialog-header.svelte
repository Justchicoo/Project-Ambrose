<!-- Project Ambrose by Imjustchico: The dialog header part of the dialog component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { cn, type WithElementRef } from "$lib/utils.js";
    import type { HTMLAttributes } from "svelte/elements";

    let { ref = $bindable(null), class: className, children, ...restProps }: WithElementRef<HTMLAttributes<HTMLDivElement>> = $props();
</script>

<div bind:this={ref} data-slot="dialog-header" class={cn("gap-2 flex flex-col", className)} {...restProps}>
    {@render children?.()}
</div>
