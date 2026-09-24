<!-- Project Ambrose by Imjustchico: Progress with its real numbers beside it, and an indeterminate bar only while an operation with no knowable total is running. -->
<script lang="ts">
    import { Progress } from "bits-ui";
    import { classes } from "../internal/classes";

    type Props = {
        label: string;
        value?: number;
        max?: number;
        detail?: string;
        indeterminate?: boolean;
        tone?: "action" | "healthy" | "waiting" | "wrong";
        class?: string;
    };

    let { label, value = 0, max = 100, detail, indeterminate = false, tone = "action", class: extra }: Props = $props();

    const tones: Record<string, string> = {
        action: "bg-fill-action",
        healthy: "bg-fill-healthy",
        waiting: "bg-fill-action",
        wrong: "bg-fill-danger",
    };
    const percent = $derived(max > 0 ? Math.round((Math.min(Math.max(value, 0), max) / max) * 100) : 0);
</script>

<div class={classes("flex flex-col gap-6", extra)}>
    <div class="flex items-baseline justify-between gap-16">
        <span class="text-13 text-fg-body">{label}</span>
        <span class="ambrose-mono text-12 text-fg-muted">{detail ?? (indeterminate ? "Running" : `${percent}%`)}</span>
    </div>
    <Progress.Root
        value={indeterminate ? null : value}
        {max}
        aria-label={label}
        class="h-8 w-full overflow-hidden rounded-pill bg-surface-sunken"
    >
        {#if indeterminate}
            <div class={classes("ambrose-indeterminate h-full w-1/3 rounded-pill", tones[tone])}></div>
        {:else}
            <div class={classes("ambrose-panel h-full rounded-pill", tones[tone])} style="width: {percent}%"></div>
        {/if}
    </Progress.Root>
</div>
