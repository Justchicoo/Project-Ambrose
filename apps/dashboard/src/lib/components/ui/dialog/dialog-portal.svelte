<!-- Project Ambrose by Imjustchico: The dialog portal part of the dialog component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { Dialog as DialogPrimitive } from "bits-ui";

    let { ...restProps }: DialogPrimitive.PortalProps = $props();
</script>

<DialogPrimitive.Portal {...restProps} />
