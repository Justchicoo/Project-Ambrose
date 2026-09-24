<!-- Project Ambrose by Imjustchico: The breadcrumb separator part of the breadcrumb component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import ChevronRightIcon from "@lucide/svelte/icons/chevron-right";
    import { cn, type WithElementRef } from "$lib/utils.js";
    import type { HTMLLiAttributes } from "svelte/elements";

    let { ref = $bindable(null), class: className, children, ...restProps }: WithElementRef<HTMLLiAttributes> = $props();
</script>

<li
    bind:this={ref}
    data-slot="breadcrumb-separator"
    role="presentation"
    aria-hidden="true"
    class={cn("[&>svg]:size-3.5", className)}
    {...restProps}
>
    {#if children}
        {@render children?.()}
    {:else}
        <ChevronRightIcon class="cn-rtl-flip" />
    {/if}
</li>
