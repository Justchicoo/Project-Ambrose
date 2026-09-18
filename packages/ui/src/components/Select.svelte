<!-- Project Ambrose by Imjustchico: A labelled single choice, with the keyboard and screen-reader behaviour from the primitive layer and our own looks from the tokens. -->
<script lang="ts">
    import { Select } from "bits-ui";
    import { classes } from "../internal/classes";
    import Icon from "./Icon.svelte";
    import Label from "./Label.svelte";

    type Choice = { value: string; label: string; disabled?: boolean };

    type Props = {
        id: string;
        label: string;
        items: Choice[];
        value?: string;
        placeholder?: string;
        hint?: string;
        disabled?: boolean;
        class?: string;
    };

    let { id, label, items, value = $bindable(""), placeholder = "Choose", hint, disabled = false, class: extra }: Props = $props();

    const hintId = $derived(`${id}-hint`);
    const chosen = $derived(items.find((item) => item.value === value));
</script>

<div class={classes("flex flex-col gap-6", extra)}>
    <Label for={id}>{label}</Label>
    <Select.Root type="single" {items} bind:value {disabled}>
        <Select.Trigger
            {id}
            aria-describedby={hint ? hintId : undefined}
            class="ambrose-hover flex min-h-44 w-full items-center justify-between gap-8 rounded-input border border-edge-strong bg-surface-sunken px-12 text-15 text-fg-body disabled:opacity-50"
        >
            <span class={chosen ? "text-fg-body" : "text-fg-faint"}>{chosen ? chosen.label : placeholder}</span>
            <Icon name="chevron-down" size="15" />
        </Select.Trigger>
        <Select.Portal>
            <Select.Content
                class="ambrose-panel z-50 min-w-44 rounded-card border border-edge-strong bg-surface-card p-6 shadow-none"
                sideOffset={6}
            >
                <Select.Viewport>
                    {#each items as item (item.value)}
                        <Select.Item
                            value={item.value}
                            label={item.label}
                            disabled={item.disabled}
                            class="ambrose-flip flex min-h-44 cursor-default items-center justify-between gap-8 rounded-control px-12 text-15 text-fg-body data-highlighted:bg-surface-sunken data-disabled:opacity-50"
                        >
                            {#snippet children({ selected })}
                                <span>{item.label}</span>
                                {#if selected}
                                    <Icon name="check" size="15" class="text-state-healthy" />
                                {/if}
                            {/snippet}
                        </Select.Item>
                    {/each}
                </Select.Viewport>
            </Select.Content>
        </Select.Portal>
    </Select.Root>
    {#if hint}
        <p id={hintId} class="text-12 text-fg-faint">{hint}</p>
    {/if}
</div>
