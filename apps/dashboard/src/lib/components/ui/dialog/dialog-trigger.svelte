<!-- Project Ambrose by Imjustchico: The dialog trigger part of the dialog component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { Dialog as DialogPrimitive } from "bits-ui";

    let { ref = $bindable(null), type = "button", ...restProps }: DialogPrimitive.TriggerProps = $props();
</script>

<DialogPrimitive.Trigger bind:ref data-slot="dialog-trigger" {type} {...restProps} />
