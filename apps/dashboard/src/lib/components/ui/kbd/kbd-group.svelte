<!-- Project Ambrose by Imjustchico: The keyboard key group part of the keyboard key component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { cn, type WithElementRef } from "$lib/utils.js";
    import type { HTMLAttributes } from "svelte/elements";

    let { ref = $bindable(null), class: className, children, ...restProps }: WithElementRef<HTMLAttributes<HTMLElement>> = $props();
</script>

<kbd bind:this={ref} data-slot="kbd-group" class={cn("gap-1 inline-flex items-center", className)} {...restProps}>
    {@render children?.()}
</kbd>
