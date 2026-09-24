<!-- Project Ambrose by Imjustchico: The tooltip component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts" generics="T = never">
    import { Tooltip as TooltipPrimitive } from "bits-ui";

    let { open = $bindable(false), ...restProps }: TooltipPrimitive.RootProps<T> = $props();
</script>

<TooltipPrimitive.Root bind:open {...restProps} />
