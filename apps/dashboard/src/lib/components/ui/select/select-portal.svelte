<!-- Project Ambrose by Imjustchico: The select portal part of the select component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { Select as SelectPrimitive } from "bits-ui";

    let { ...restProps }: SelectPrimitive.PortalProps = $props();
</script>

<SelectPrimitive.Portal {...restProps} />
