<!-- Project Ambrose by Imjustchico: The one action that matters, the quiet actions beside it and the destructive one, as a real button or a real link, never smaller than a touch target. -->
<script lang="ts">
    import type { Snippet } from "svelte";
    import { classes } from "../internal/classes";
    import Icon from "./Icon.svelte";
    import type { IconName } from "../icons/icons";

    type Props = {
        variant?: "action" | "quiet" | "danger" | "ghost";
        size?: "regular" | "wide";
        type?: "button" | "submit" | "reset";
        href?: string;
        icon?: IconName;
        disabled?: boolean;
        busy?: boolean;
        busyLabel?: string;
        title?: string;
        id?: string;
        class?: string;
        onclick?: (event: MouseEvent) => void;
        children: Snippet;
        [attribute: string]: unknown;
    };

    let {
        variant = "quiet",
        size = "regular",
        type = "button",
        href,
        icon,
        disabled = false,
        busy = false,
        busyLabel = "Working",
        title,
        id,
        class: extra,
        onclick,
        children,
        ...rest
    }: Props = $props();

    const looks: Record<string, string> = {
        action: "ambrose-inset bg-fill-action text-on-fill font-semibold ring-1 ring-inset ring-fill-action-pressed hover:brightness-110 active:bg-fill-action-pressed active:brightness-100",
        quiet: "bg-surface-card text-fg-body font-medium ring-1 ring-inset ring-edge-strong/70 hover:bg-surface-sunken hover:ring-action/50",
        danger: "bg-state-wrong/10 text-state-wrong font-medium ring-1 ring-inset ring-state-wrong/30 hover:bg-state-wrong/20",
        ghost: "text-fg-muted font-medium hover:bg-fg-muted/10 hover:text-fg-body",
    };
    const shapes: Record<string, string> = {
        regular: "px-14 rounded-input",
        wide: "px-28 text-17 rounded-action",
    };
    const shared =
        "ambrose-hover inline-flex min-h-44 items-center justify-center gap-8 text-15 select-none disabled:opacity-50 disabled:pointer-events-none [&_svg]:size-16 [&_svg]:shrink-0";
    const all = $derived(classes(shared, looks[variant], shapes[size], extra));
</script>

{#if href}
    <a {...rest} {id} {href} {title} class={all} aria-disabled={disabled ? "true" : undefined}>
        {#if icon}<Icon name={icon} />{/if}
        {@render children()}
    </a>
{:else}
    <button {...rest} {id} {type} {title} {onclick} class={all} disabled={disabled || busy} aria-busy={busy ? "true" : undefined}>
        {#if icon}<Icon name={icon} />{/if}
        {#if busy}{busyLabel}{:else}{@render children()}{/if}
    </button>
{/if}
