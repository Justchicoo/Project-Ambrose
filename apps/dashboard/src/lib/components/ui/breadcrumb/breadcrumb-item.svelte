<!-- Project Ambrose by Imjustchico: The breadcrumb item part of the breadcrumb component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { cn, type WithElementRef } from "$lib/utils.js";
    import type { HTMLLiAttributes } from "svelte/elements";

    let { ref = $bindable(null), class: className, children, ...restProps }: WithElementRef<HTMLLiAttributes> = $props();
</script>

<li bind:this={ref} data-slot="breadcrumb-item" class={cn("gap-1 inline-flex items-center", className)} {...restProps}>
    {@render children?.()}
</li>
