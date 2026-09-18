<!-- Project Ambrose by Imjustchico: Text a screen reader reads and the eye does not, which is how an icon-only control carries its label. -->
<script lang="ts">
    import type { Snippet } from "svelte";

    type Props = {
        children: Snippet;
    };

    let { children }: Props = $props();
</script>

<span class="absolute h-4 w-4 overflow-hidden whitespace-nowrap border-0 p-0" style:clip-path="inset(50%)" style:margin="-1px">
    {@render children()}
</span>
