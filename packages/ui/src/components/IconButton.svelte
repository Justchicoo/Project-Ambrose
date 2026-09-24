<!-- Project Ambrose by Imjustchico: A control that shows only an icon, which is why its label is required rather than optional. -->
<script lang="ts">
    import { classes } from "../internal/classes";
    import Icon from "./Icon.svelte";
    import VisuallyHidden from "./VisuallyHidden.svelte";
    import type { IconName } from "../icons/icons";

    type Props = {
        icon: IconName;
        label: string;
        variant?: "quiet" | "ghost" | "danger";
        disabled?: boolean;
        pressed?: boolean;
        id?: string;
        class?: string;
        onclick?: (event: MouseEvent) => void;
        [attribute: string]: unknown;
    };

    let { icon, label, variant = "ghost", disabled = false, pressed, id, class: extra, onclick, ...rest }: Props = $props();

    const looks: Record<string, string> = {
        quiet: "bg-surface-sunken border border-edge-strong text-fg-body hover:border-action",
        ghost: "text-fg-muted hover:text-fg-body",
        danger: "text-state-wrong hover:opacity-80",
    };
</script>

<button
    {...rest}
    {id}
    type="button"
    {onclick}
    {disabled}
    title={label}
    aria-pressed={pressed === undefined ? undefined : pressed ? "true" : "false"}
    class={classes(
        "ambrose-hover inline-flex min-h-44 min-w-44 items-center justify-center rounded-input disabled:opacity-50 disabled:pointer-events-none",
        looks[variant],
        extra,
    )}
>
    <Icon name={icon} size="17" />
    <VisuallyHidden>{label}</VisuallyHidden>
</button>
