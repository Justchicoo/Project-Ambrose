<!-- Project Ambrose by Imjustchico: The tooltip trigger part of the tooltip component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts" generics="T = never">
    import { Tooltip as TooltipPrimitive } from "bits-ui";

    let { ref = $bindable(null), ...restProps }: TooltipPrimitive.TriggerProps<T> = $props();
</script>

<TooltipPrimitive.Trigger bind:ref data-slot="tooltip-trigger" {...restProps} />
