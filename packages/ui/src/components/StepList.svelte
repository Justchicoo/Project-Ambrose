<!-- Project Ambrose by Imjustchico: What a long operation is doing, as a list of steps each with its own state and its own real figure, which is what stands in for a spinner everywhere in Ambrose. -->
<script lang="ts">
    import { classes } from "../internal/classes";
    import StateDot from "./StateDot.svelte";

    type Step = {
        id: string;
        label: string;
        state: "healthy" | "waiting" | "wrong" | "unknown";
        word: string;
        detail?: string;
    };

    type Props = {
        label: string;
        steps: Step[];
        class?: string;
    };

    let { label, steps, class: extra }: Props = $props();
</script>

<ol aria-label={label} class={classes("flex list-none flex-col gap-2 p-0", extra)}>
    {#each steps as step (step.id)}
        <li class="flex min-h-44 items-center justify-between gap-16 rounded-input px-12 py-8 odd:bg-surface-sunken">
            <div class="flex min-w-0 flex-col gap-2">
                <span class="truncate text-13 text-fg-body">{step.label}</span>
                <StateDot state={step.state} word={step.word} size="11" live={step.state === "waiting"} />
            </div>
            {#if step.detail}
                <span class="ambrose-mono shrink-0 text-13 text-fg-muted">{step.detail}</span>
            {/if}
        </li>
    {/each}
</ol>
