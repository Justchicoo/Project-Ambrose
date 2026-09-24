<!-- Project Ambrose by Imjustchico: The avatar image part of the avatar component from shadcn-svelte, copied in and owned, styled from the panel stylesheet's theme variables. -->
<script lang="ts">
    import { Avatar as AvatarPrimitive } from "bits-ui";
    import { cn } from "$lib/utils.js";

    let { ref = $bindable(null), class: className, ...restProps }: AvatarPrimitive.ImageProps = $props();
</script>

<AvatarPrimitive.Image
    bind:ref
    data-slot="avatar-image"
    class={cn("rounded-full aspect-square size-full object-cover", className)}
    {...restProps}
/>
