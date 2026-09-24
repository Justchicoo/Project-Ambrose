<!-- Project Ambrose by Imjustchico: The command loading part of the command component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { Command as CommandPrimitive } from "bits-ui";

    let { ref = $bindable(null), ...restProps }: CommandPrimitive.LoadingProps = $props();
</script>

<CommandPrimitive.Loading bind:ref {...restProps} />
