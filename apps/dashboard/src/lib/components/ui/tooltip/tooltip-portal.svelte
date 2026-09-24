<!-- Project Ambrose by Imjustchico: The tooltip portal part of the tooltip component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { Tooltip as TooltipPrimitive } from "bits-ui";

    let { ...restProps }: TooltipPrimitive.PortalProps = $props();
</script>

<TooltipPrimitive.Portal {...restProps} />
