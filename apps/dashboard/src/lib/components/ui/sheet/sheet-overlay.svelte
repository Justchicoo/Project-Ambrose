<!-- Project Ambrose by Imjustchico: The sheet overlay part of the sheet component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { Dialog as SheetPrimitive } from "bits-ui";
    import { cn } from "$lib/utils.js";

    let { ref = $bindable(null), class: className, ...restProps }: SheetPrimitive.OverlayProps = $props();
</script>

<SheetPrimitive.Overlay
    bind:ref
    data-slot="sheet-overlay"
    class={cn(
        "bg-black/10 supports-backdrop-filter:backdrop-blur-xs fixed inset-0 z-50 duration-100 data-open:animate-in data-open:fade-in-0 data-closed:animate-out data-closed:fade-out-0",
        className,
    )}
    {...restProps}
/>
