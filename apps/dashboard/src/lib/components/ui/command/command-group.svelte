<!-- Project Ambrose by Imjustchico: The command group part of the command component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { Command as CommandPrimitive, useId } from "bits-ui";
    import { cn } from "$lib/utils.js";

    let {
        ref = $bindable(null),
        class: className,
        children,
        heading,
        value,
        ...restProps
    }: CommandPrimitive.GroupProps & {
        heading?: string;
    } = $props();
</script>

<CommandPrimitive.Group
    bind:ref
    data-slot="command-group"
    class={cn(
        "text-foreground **:[[cmdk-group-heading]]:text-muted-foreground overflow-hidden p-1 **:[[cmdk-group-heading]]:px-2 **:[[cmdk-group-heading]]:py-1.5 **:[[cmdk-group-heading]]:text-xs **:[[cmdk-group-heading]]:font-medium",
        className,
    )}
    value={value ?? heading ?? `----${useId()}`}
    {...restProps}
>
    {#if heading}
        <CommandPrimitive.GroupHeading class="px-2 py-1.5 text-xs font-medium text-muted-foreground">
            {heading}
        </CommandPrimitive.GroupHeading>
    {/if}
    <CommandPrimitive.GroupItems {children} />
</CommandPrimitive.Group>
