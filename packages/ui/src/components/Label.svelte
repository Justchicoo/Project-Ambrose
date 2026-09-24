<!-- Project Ambrose by Imjustchico: The 11 px uppercase label that names a group or a field, tied to the control it names when one is given. -->
<script lang="ts">
    import type { Snippet } from "svelte";
    import { classes } from "../internal/classes";

    type Props = {
        for?: string;
        id?: string;
        class?: string;
        children: Snippet;
    };

    let { for: control, id, class: extra, children }: Props = $props();
</script>

{#if control}
    <label {id} for={control} class={classes("ambrose-label block", extra)}>{@render children()}</label>
{:else}
    <span {id} class={classes("ambrose-label block", extra)}>{@render children()}</span>
{/if}
