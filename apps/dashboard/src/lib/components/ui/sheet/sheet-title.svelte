<!-- Project Ambrose by Imjustchico: The sheet title part of the sheet component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { Dialog as SheetPrimitive } from "bits-ui";
    import { cn } from "$lib/utils.js";

    let { ref = $bindable(null), class: className, ...restProps }: SheetPrimitive.TitleProps = $props();
</script>

<SheetPrimitive.Title bind:ref data-slot="sheet-title" class={cn("text-foreground text-base font-medium", className)} {...restProps} />
