<!-- Project Ambrose by Imjustchico: The context menu portal part of the context menu component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { ContextMenu as ContextMenuPrimitive } from "bits-ui";

    let { ...restProps }: ContextMenuPrimitive.PortalProps = $props();
</script>

<ContextMenuPrimitive.Portal {...restProps} />
