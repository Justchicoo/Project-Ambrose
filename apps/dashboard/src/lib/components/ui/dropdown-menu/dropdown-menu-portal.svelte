<!-- Project Ambrose by Imjustchico: The dropdown menu portal part of the dropdown menu component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { DropdownMenu as DropdownMenuPrimitive } from "bits-ui";

    let { ...restProps }: DropdownMenuPrimitive.PortalProps = $props();
</script>

<DropdownMenuPrimitive.Portal {...restProps} />
