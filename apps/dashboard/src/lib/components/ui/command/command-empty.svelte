<!-- Project Ambrose by Imjustchico: The command empty part of the command component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { Command as CommandPrimitive } from "bits-ui";
    import { cn } from "$lib/utils.js";

    let { ref = $bindable(null), class: className, ...restProps }: CommandPrimitive.EmptyProps = $props();
</script>

<CommandPrimitive.Empty bind:ref data-slot="command-empty" class={cn("py-6 text-center text-sm", className)} {...restProps} />
