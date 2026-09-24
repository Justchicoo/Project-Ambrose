<!-- Project Ambrose by Imjustchico: The breadcrumb list part of the breadcrumb component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { cn, type WithElementRef } from "$lib/utils.js";
    import type { HTMLOlAttributes } from "svelte/elements";

    let { ref = $bindable(null), class: className, children, ...restProps }: WithElementRef<HTMLOlAttributes> = $props();
</script>

<ol
    bind:this={ref}
    data-slot="breadcrumb-list"
    class={cn("text-muted-foreground gap-1.5 text-sm flex flex-wrap items-center wrap-break-word", className)}
    {...restProps}
>
    {@render children?.()}
</ol>
