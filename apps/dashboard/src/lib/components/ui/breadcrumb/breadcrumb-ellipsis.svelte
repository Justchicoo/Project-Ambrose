<!-- Project Ambrose by Imjustchico: The breadcrumb ellipsis part of the breadcrumb component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import MoreHorizontalIcon from "@lucide/svelte/icons/more-horizontal";
    import { cn, type WithElementRef, type WithoutChildren } from "$lib/utils.js";
    import type { HTMLAttributes } from "svelte/elements";

    let {
        ref = $bindable(null),
        class: className,
        ...restProps
    }: WithoutChildren<WithElementRef<HTMLAttributes<HTMLSpanElement>>> = $props();
</script>

<span
    bind:this={ref}
    data-slot="breadcrumb-ellipsis"
    role="presentation"
    aria-hidden="true"
    class={cn("size-5 [&>svg]:size-4 flex items-center justify-center", className)}
    {...restProps}
>
    <MoreHorizontalIcon />
    <span class="sr-only">More</span>
</span>
