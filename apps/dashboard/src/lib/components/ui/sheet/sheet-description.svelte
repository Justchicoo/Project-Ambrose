<!-- Project Ambrose by Imjustchico: The sheet description part of the sheet component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { Dialog as SheetPrimitive } from "bits-ui";
    import { cn } from "$lib/utils.js";

    let { ref = $bindable(null), class: className, ...restProps }: SheetPrimitive.DescriptionProps = $props();
</script>

<SheetPrimitive.Description bind:ref data-slot="sheet-description" class={cn("text-muted-foreground text-sm", className)} {...restProps} />
