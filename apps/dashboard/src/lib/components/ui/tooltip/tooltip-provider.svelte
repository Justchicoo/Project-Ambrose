<!-- Project Ambrose by Imjustchico: The tooltip provider part of the tooltip component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { Tooltip as TooltipPrimitive } from "bits-ui";

    let { delayDuration = 0, ...restProps }: TooltipPrimitive.ProviderProps = $props();
</script>

<TooltipPrimitive.Provider {delayDuration} {...restProps} />
