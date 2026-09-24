<!-- Project Ambrose by Imjustchico: The dialog title part of the dialog component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { Dialog as DialogPrimitive } from "bits-ui";
    import { cn } from "$lib/utils.js";

    let { ref = $bindable(null), class: className, ...restProps }: DialogPrimitive.TitleProps = $props();
</script>

<DialogPrimitive.Title bind:ref data-slot="dialog-title" class={cn("text-base leading-none font-medium", className)} {...restProps} />
