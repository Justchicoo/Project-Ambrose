<!-- Project Ambrose by Imjustchico: The select value part of the select component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { Select as SelectPrimitive } from "bits-ui";

    let { ref = $bindable(null), ...restProps }: SelectPrimitive.ValueProps = $props();
</script>

<SelectPrimitive.Value bind:ref data-slot="select-value" {...restProps} />
