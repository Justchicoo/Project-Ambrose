<!-- Project Ambrose by Imjustchico: A checkbox with its own label, at a touch-target size, showing its checked and indeterminate states in the tokens. -->
<script lang="ts">
    import { Checkbox } from "bits-ui";
    import { classes } from "../internal/classes";
    import Icon from "./Icon.svelte";

    type Props = {
        id: string;
        label: string;
        checked?: boolean;
        indeterminate?: boolean;
        disabled?: boolean;
        hint?: string;
        class?: string;
    };

    let { id, label, checked = $bindable(false), indeterminate = false, disabled = false, hint, class: extra }: Props = $props();

    const hintId = $derived(`${id}-hint`);
</script>

<div class={classes("flex items-start gap-10", extra)}>
    <Checkbox.Root
        {id}
        bind:checked
        {disabled}
        {indeterminate}
        aria-describedby={hint ? hintId : undefined}
        class="ambrose-flip mt-4 flex h-20 w-20 shrink-0 items-center justify-center rounded-control border border-edge-strong bg-surface-sunken data-[state=checked]:bg-fill-action data-[state=checked]:text-on-fill disabled:opacity-50"
    >
        {#snippet children({ checked: on, indeterminate })}
            {#if indeterminate}
                <Icon name="minus" size="13" />
            {:else if on}
                <Icon name="check" size="13" />
            {/if}
        {/snippet}
    </Checkbox.Root>
    <div class="flex min-h-44 flex-col justify-center gap-4">
        <label for={id} class="text-15 text-fg-body">{label}</label>
        {#if hint}
            <p id={hintId} class="text-12 text-fg-faint">{hint}</p>
        {/if}
    </div>
</div>
