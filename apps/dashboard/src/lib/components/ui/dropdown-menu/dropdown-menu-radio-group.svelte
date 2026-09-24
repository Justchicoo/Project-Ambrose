<!-- Project Ambrose by Imjustchico: The dropdown menu radio group part of the dropdown menu component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { DropdownMenu as DropdownMenuPrimitive } from "bits-ui";

    let { ref = $bindable(null), value = $bindable(), ...restProps }: DropdownMenuPrimitive.RadioGroupProps = $props();
</script>

<DropdownMenuPrimitive.RadioGroup bind:ref bind:value data-slot="dropdown-menu-radio-group" {...restProps} />
