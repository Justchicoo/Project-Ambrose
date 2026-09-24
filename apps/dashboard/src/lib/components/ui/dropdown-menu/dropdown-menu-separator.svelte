<!-- Project Ambrose by Imjustchico: The dropdown menu separator part of the dropdown menu component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { DropdownMenu as DropdownMenuPrimitive } from "bits-ui";
    import { cn } from "$lib/utils.js";

    let { ref = $bindable(null), class: className, ...restProps }: DropdownMenuPrimitive.SeparatorProps = $props();
</script>

<DropdownMenuPrimitive.Separator
    bind:ref
    data-slot="dropdown-menu-separator"
    class={cn("bg-border -mx-1 my-1 h-px", className)}
    {...restProps}
/>
