<!-- Project Ambrose by Imjustchico: The select component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { Select as SelectPrimitive } from "bits-ui";

    let { open = $bindable(false), value = $bindable(), ...restProps }: SelectPrimitive.RootProps = $props();
</script>

<SelectPrimitive.Root bind:open bind:value={value as never} {...restProps} />
