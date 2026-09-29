<!-- Project Ambrose by Imjustchico: The context menu separator part of the context menu component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { ContextMenu as ContextMenuPrimitive } from "bits-ui";
    import { cn } from "$lib/utils.js";

    let { ref = $bindable(null), class: className, ...restProps }: ContextMenuPrimitive.SeparatorProps = $props();
</script>

<ContextMenuPrimitive.Separator
    bind:ref
    data-slot="context-menu-separator"
    class={cn("bg-border -mx-1 my-1 h-px", className)}
    {...restProps}
/>
