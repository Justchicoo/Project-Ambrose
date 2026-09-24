<!-- Project Ambrose by Imjustchico: The dropdown menu group heading part of the dropdown menu component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { DropdownMenu as DropdownMenuPrimitive } from "bits-ui";
    import { cn } from "$lib/utils.js";
    import type { ComponentProps } from "svelte";

    let {
        ref = $bindable(null),
        class: className,
        inset,
        ...restProps
    }: ComponentProps<typeof DropdownMenuPrimitive.GroupHeading> & {
        inset?: boolean;
    } = $props();
</script>

<DropdownMenuPrimitive.GroupHeading
    bind:ref
    data-slot="dropdown-menu-group-heading"
    data-inset={inset}
    class={cn("text-muted-foreground px-1.5 py-1 text-xs font-medium data-inset:pl-7", className)}
    {...restProps}
/>
