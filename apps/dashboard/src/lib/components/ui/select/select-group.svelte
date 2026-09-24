<!-- Project Ambrose by Imjustchico: The select group part of the select component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { Select as SelectPrimitive } from "bits-ui";
    import { cn } from "$lib/utils.js";

    let { ref = $bindable(null), class: className, ...restProps }: SelectPrimitive.GroupProps = $props();
</script>

<SelectPrimitive.Group bind:ref data-slot="select-group" class={cn("scroll-my-1 p-1", className)} {...restProps} />
