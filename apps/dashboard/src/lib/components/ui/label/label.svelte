<!-- Project Ambrose by Imjustchico: The label component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { Label as LabelPrimitive } from "bits-ui";
    import { cn } from "$lib/utils.js";

    let { ref = $bindable(null), class: className, ...restProps }: LabelPrimitive.RootProps = $props();
</script>

<LabelPrimitive.Root
    bind:ref
    data-slot="label"
    class={cn(
        "gap-2 text-sm leading-none font-medium group-data-[disabled=true]:opacity-50 peer-disabled:opacity-50 flex items-center select-none group-data-[disabled=true]:pointer-events-none peer-disabled:cursor-not-allowed",
        className,
    )}
    {...restProps}
/>
