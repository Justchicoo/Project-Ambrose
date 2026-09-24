<!-- Project Ambrose by Imjustchico: The sheet portal part of the sheet component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { Dialog as SheetPrimitive } from "bits-ui";

    let { ...restProps }: SheetPrimitive.PortalProps = $props();
</script>

<SheetPrimitive.Portal {...restProps} />
