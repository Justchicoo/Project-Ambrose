<!-- Project Ambrose by Imjustchico: A labelled text input on the sunken fill, with its hint and its error tied to it by id so a screen reader reads both. -->
<script lang="ts">
    import { classes } from "../internal/classes";
    import Label from "./Label.svelte";

    type Props = {
        id: string;
        label: string;
        value?: string;
        type?: "text" | "password" | "email" | "search" | "url" | "number";
        placeholder?: string;
        hint?: string;
        error?: string;
        required?: boolean;
        disabled?: boolean;
        readonly?: boolean;
        mono?: boolean;
        class?: string;
        oninput?: (event: Event) => void;
    };

    let {
        id,
        label,
        value = $bindable(""),
        type = "text",
        placeholder,
        hint,
        error,
        required = false,
        disabled = false,
        readonly = false,
        mono = false,
        class: extra,
        oninput,
    }: Props = $props();

    const hintId = $derived(`${id}-hint`);
    const errorId = $derived(`${id}-error`);
    const described = $derived([hint ? hintId : null, error ? errorId : null].filter(Boolean).join(" ") || undefined);
</script>

<div class={classes("flex flex-col gap-6", extra)}>
    <Label for={id}>{label}</Label>
    <input
        {id}
        {type}
        {placeholder}
        {required}
        {disabled}
        {readonly}
        {oninput}
        bind:value
        aria-describedby={described}
        aria-invalid={error ? "true" : undefined}
        class={classes(
            "ambrose-hover min-h-44 w-full rounded-input border border-edge-strong bg-surface-sunken px-12 text-15 text-fg-body placeholder:text-fg-faint disabled:opacity-50",
            mono ? "ambrose-mono" : null,
            error ? "border-state-wrong" : null,
        )}
    />
    {#if hint}
        <p id={hintId} class="text-12 text-fg-faint">{hint}</p>
    {/if}
    {#if error}
        <p id={errorId} class="text-12 text-state-wrong">{error}</p>
    {/if}
</div>
