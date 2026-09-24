<!-- Project Ambrose by Imjustchico: One series drawn small with no axes and no animation, refilled in place as readings arrive, always beside a table of the same numbers, because a canvas is not something a screen reader can read. -->
<script lang="ts">
    import { untrack } from "svelte";
    import { classes } from "../internal/classes";
    import { mountPlot, type Plot } from "../internal/plot";
    import { seriesColors } from "../tokens/tokens";

    type Props = {
        label: string;
        times: number[];
        values: (number | null)[];
        unit?: string;
        theme?: "dark" | "light";
        height?: number;
        class?: string;
    };

    let { label, times, values, unit, theme = "dark", height = 44, class: extra }: Props = $props();

    let holder = $state<HTMLDivElement | undefined>(undefined);
    let width = $state(160);
    let plot = $state.raw<Plot | undefined>(undefined);

    const stroke = $derived(seriesColors[theme][0]);

    $effect(() => {
        if (!holder) {
            return;
        }
        let frame = 0;
        const observer = new ResizeObserver((entries) => {
            const measured = Math.max(40, Math.round(entries[0].contentRect.width));
            cancelAnimationFrame(frame);
            frame = requestAnimationFrame(() => {
                width = measured;
            });
        });
        observer.observe(holder);
        return () => {
            cancelAnimationFrame(frame);
            observer.disconnect();
        };
    });

    $effect(() => {
        if (!holder) {
            return;
        }
        const target = holder;
        const color = stroke;
        const created = untrack(() =>
            mountPlot(target, {
                width,
                height,
                times,
                series: [{ values, color, fill: false, shown: true }],
                top: null,
                time: false,
                cursor: null,
                drawn: null,
            }),
        );
        plot = created;
        return () => {
            created.destroy();
            plot = undefined;
        };
    });

    $effect(() => {
        plot?.update(times, [{ values, color: stroke, fill: false, shown: true }], null);
    });

    $effect(() => {
        plot?.resize(width, height);
    });
</script>

<figure class={classes("sparkline", extra)}>
    <figcaption class="hidden-text">{label}</figcaption>
    <div bind:this={holder} aria-hidden="true" class="holder"></div>
    <table class="hidden-text">
        <caption>{label}</caption>
        <thead>
            <tr><th scope="col">Point</th><th scope="col">{unit ?? "Value"}</th></tr>
        </thead>
        <tbody>
            {#each times as time, index (time)}
                <tr><td>{time}</td><td>{values[index] ?? "no reading"}</td></tr>
            {/each}
        </tbody>
    </table>
</figure>

<style>
    .sparkline {
        display: flex;
        flex-direction: column;
        margin: 0;
    }

    .holder {
        width: 100%;
    }

    .hidden-text {
        position: absolute;
        width: 1px;
        height: 1px;
        overflow: hidden;
        clip-path: inset(50%);
        white-space: nowrap;
    }
</style>
