<!-- Project Ambrose by Imjustchico: One series drawn small with no axes and no animation, always beside a table of the same numbers, because a canvas is not something a screen reader can read. -->
<script lang="ts">
    import uPlot from "uplot";
    import { classes } from "../internal/classes";
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
    let chart: uPlot | undefined;

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
        const data: uPlot.AlignedData = [times, values];
        chart?.destroy();
        chart = new uPlot(
            {
                width,
                height,
                cursor: { show: false },
                legend: { show: false },
                axes: [{ show: false }, { show: false }],
                scales: { x: { time: false } },
                series: [{}, { stroke, width: 1.5, spanGaps: false, points: { show: false } }],
            },
            data,
            holder,
        );
        return () => {
            chart?.destroy();
            chart = undefined;
        };
    });
</script>

<figure class={classes("flex flex-col gap-6", extra)}>
    <figcaption class="sr-only">{label}</figcaption>
    <div bind:this={holder} aria-hidden="true" class="w-full"></div>
    <table class="sr-only">
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
