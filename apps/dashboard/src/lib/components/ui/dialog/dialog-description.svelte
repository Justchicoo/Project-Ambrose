<!-- Project Ambrose by Imjustchico: The dialog description part of the dialog component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { Dialog as DialogPrimitive } from "bits-ui";
    import { cn } from "$lib/utils.js";

    let { ref = $bindable(null), class: className, ...restProps }: DialogPrimitive.DescriptionProps = $props();
</script>

<DialogPrimitive.Description
    bind:ref
    data-slot="dialog-description"
    class={cn("text-muted-foreground *:[a]:hover:text-foreground text-sm *:[a]:underline *:[a]:underline-offset-3", className)}
    {...restProps}
/>
