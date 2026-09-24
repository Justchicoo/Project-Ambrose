<!-- Project Ambrose by Imjustchico: The command separator part of the command component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { Command as CommandPrimitive } from "bits-ui";
    import { cn } from "$lib/utils.js";

    let { ref = $bindable(null), class: className, ...restProps }: CommandPrimitive.SeparatorProps = $props();
</script>

<CommandPrimitive.Separator bind:ref data-slot="command-separator" class={cn("bg-border -mx-1 h-px", className)} {...restProps} />
