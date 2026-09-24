<!-- Project Ambrose by Imjustchico: The dialog close part of the dialog component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { Dialog as DialogPrimitive } from "bits-ui";

    let { ref = $bindable(null), type = "button", ...restProps }: DialogPrimitive.CloseProps = $props();
</script>

<DialogPrimitive.Close bind:ref data-slot="dialog-close" {type} {...restProps} />
