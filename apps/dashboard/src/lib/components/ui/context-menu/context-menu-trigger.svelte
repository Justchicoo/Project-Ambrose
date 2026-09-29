<!-- Project Ambrose by Imjustchico: The context menu trigger part of the context menu component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { ContextMenu as ContextMenuPrimitive } from "bits-ui";

    let { ref = $bindable(null), ...restProps }: ContextMenuPrimitive.TriggerProps = $props();
</script>

<ContextMenuPrimitive.Trigger bind:ref data-slot="context-menu-trigger" {...restProps} />
