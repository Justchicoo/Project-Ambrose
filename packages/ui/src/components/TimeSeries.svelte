<!-- Project Ambrose by Imjustchico: Values over time drawn with uPlot, with everything that carries meaning in real markup over the canvas: guide lines and their values, time labels, a crosshair and tooltip, a legend of buttons that hide and show each series, gaps where a reading is missing, a fixed height or the height of its container, and a table view of the same numbers that a button switches to and a screen reader always has. -->
<script lang="ts">
    import { untrack } from "svelte";
    import { classes } from "../internal/classes";
    import { mountPlot, type Plot, type PlotSeries } from "../internal/plot";
    import { seriesColors } from "../tokens/tokens";

    type Series = { label: string; values: (number | null)[] };

    type Props = {
        label: string;
        unit: string;
        times: number[];
        series: Series[];
        theme?: "dark" | "light";
        height?: number;
        fill?: boolean;
        class?: string;
    };

    let { label, unit, times, series, theme = "dark", height = 208, fill = false, class: extra }: Props = $props();

    let holder = $state<HTMLDivElement | undefined>(undefined);
    let width = $state(0);
    let measured = $state(0);
    let plot = $state.raw<Plot | undefined>(undefined);
    let hovered = $state<number | null>(null);
    let hidden = $state<number[]>([]);
    let view = $state<"chart" | "table">("chart");
    let drawn = $state(0);

    const palette = $derived(seriesColors[theme]);
    const shape = $derived(`${theme}:${series.length}`);
    const step = $derived.by(() => {
        const highest = Math.max(
            1,
            ...series.flatMap((entry, index) => (hidden.includes(index) ? [] : entry.values.filter((value) => value !== null))),
        );
        const rough = (highest * 1.1) / 4;
        const magnitude = 10 ** Math.floor(Math.log10(rough));
        const steps = [1, 1.2, 1.5, 2, 2.5, 3, 4, 5, 6, 8, 10].map((multiple) => multiple * magnitude);
        return steps.find((candidate) => candidate >= rough && (magnitude < 1 || Number.isInteger(candidate))) ?? 10 * magnitude;
    });
    const top = $derived(step * 4);
    const plotHeight = $derived(fill ? Math.max(120, measured) : height);
    const guides = $derived([4, 3, 2, 1, 0].map((share) => Number((share * step).toPrecision(6))));
    const span = $derived(times.length > 1 ? times[times.length - 1] - times[0] : 0);
    const ticks = $derived(times.length > 1 ? [0, 0.25, 0.5, 0.75, 1].map((share) => times[0] + share * span) : []);
    const short = $derived(
        new Intl.DateTimeFormat(
            undefined,
            span > 36 * 3600 ? { weekday: "short", hour: "2-digit", minute: "2-digit" } : { hour: "2-digit", minute: "2-digit" },
        ),
    );
    const spacing = $derived(times.length > 1 ? span / (times.length - 1) : 0);
    const long = $derived(
        new Intl.DateTimeFormat(undefined, {
            weekday: "short",
            day: "numeric",
            month: "short",
            hour: "2-digit",
            minute: "2-digit",
            second: spacing < 60 ? "2-digit" : undefined,
        }),
    );
    const number = new Intl.NumberFormat(undefined, { maximumFractionDigits: 2 });

    const drawnSeries = $derived<PlotSeries[]>(
        series.map((entry, index) => ({
            values: entry.values,
            color: palette[index % palette.length],
            fill: series.length === 1,
            shown: !hidden.includes(index),
        })),
    );
    const guideTops = $derived.by(() => {
        void drawn;
        return plot ? guides.map((guide) => plot!.y(guide)) : [];
    });
    const tickLefts = $derived.by(() => {
        void drawn;
        return plot ? ticks.map((time) => plot!.x(time)) : [];
    });
    const area = $derived.by(() => {
        void drawn;
        return plot ? plot.area() : { left: 0, top: 0, width: 0, height: 0 };
    });

    function reading(value: number | null) {
        return value === null ? "no reading" : `${number.format(value)} ${unit}`;
    }

    function toggle(index: number) {
        hidden = hidden.includes(index) ? hidden.filter((entry) => entry !== index) : [...hidden, index];
    }

    $effect(() => {
        if (!holder) {
            return;
        }
        let frame = 0;
        const observer = new ResizeObserver((entries) => {
            const box = entries[0].contentRect;
            cancelAnimationFrame(frame);
            frame = requestAnimationFrame(() => {
                width = Math.max(80, Math.round(box.width));
                measured = Math.round(box.height);
            });
        });
        observer.observe(holder);
        return () => {
            cancelAnimationFrame(frame);
            observer.disconnect();
        };
    });

    $effect(() => {
        if (!holder || width === 0) {
            return;
        }
        void shape;
        const target = holder;
        const created = untrack(() =>
            mountPlot(target, {
                width,
                height: plotHeight,
                times,
                series: drawnSeries,
                top,
                time: true,
                cursor: (index) => (hovered = index),
                drawn: () => (drawn += 1),
            }),
        );
        plot = created;
        return () => {
            created.destroy();
            plot = undefined;
            hovered = null;
        };
    });

    $effect(() => {
        plot?.update(times, drawnSeries, top);
    });

    $effect(() => {
        plot?.resize(width, plotHeight);
    });
</script>

<figure class={classes("time-series", extra)} class:fill>
    <figcaption class="hidden-text">{label}</figcaption>
    <div class="bar">
        {#if series.length > 1}
            <div class="legend" role="group" aria-label="Series to show">
                {#each series as entry, index (entry.label)}
                    <button type="button" class="key" aria-pressed={!hidden.includes(index)} onclick={() => toggle(index)}>
                        <span class="swatch" style:background={palette[index % palette.length]}></span>{entry.label}
                    </button>
                {/each}
            </div>
        {/if}
        <button type="button" class="view" aria-pressed={view === "table"} onclick={() => (view = view === "chart" ? "table" : "chart")}>
            Table view
        </button>
    </div>
    {#if view === "chart"}
        <div class="plot" class:grow={fill} style:height={fill ? null : `${height}px`} style:min-height={fill ? `${height}px` : null}>
            <div class="holder" bind:this={holder} aria-hidden="true"></div>
            <div class="marks" aria-hidden="true">
                {#each guides as guide, index (index)}
                    {#if guideTops[index] !== undefined}
                        <span
                            class="guide"
                            style:top={`${guideTops[index]}px`}
                            style:left={`${area.left}px`}
                            style:width={`${area.width}px`}
                        ></span>
                        <span class="value" style:top={`${guideTops[index]}px`}>{number.format(guide)}</span>
                    {/if}
                {/each}
                {#if hovered !== null && times[hovered] !== undefined && plot}
                    {@const left = plot.x(times[hovered])}
                    <span class="crosshair" style:left={`${left}px`} style:top={`${area.top}px`} style:height={`${area.height}px`}></span>
                    {#each series as entry, index (entry.label)}
                        {#if !hidden.includes(index) && entry.values[hovered] !== null}
                            <span
                                class="dot"
                                style:left={`${left}px`}
                                style:top={`${plot.y(entry.values[hovered] ?? 0)}px`}
                                style:background={palette[index % palette.length]}
                            ></span>
                        {/if}
                    {/each}
                    <div class="tip" class:flip={left > area.left + area.width * 0.6} style:left={`${left}px`}>
                        <div class="when">{long.format(new Date(times[hovered] * 1000))}</div>
                        {#each series as entry, index (entry.label)}
                            {#if !hidden.includes(index)}
                                <div class="row">
                                    <span class="swatch" style:background={palette[index % palette.length]}></span>
                                    {#if series.length > 1}<span class="name">{entry.label}</span>{/if}
                                    <span class="figure">{reading(entry.values[hovered])}</span>
                                </div>
                            {/if}
                        {/each}
                    </div>
                {/if}
            </div>
        </div>
        <div class="times" aria-hidden="true">
            {#each ticks as time, index (index)}
                {#if tickLefts[index] !== undefined}
                    <span
                        class="time"
                        class:first={index === 0}
                        class:last={index === ticks.length - 1}
                        class:minor={index % 2 === 1}
                        style:left={`${tickLefts[index]}px`}>{short.format(new Date(time * 1000))}</span
                    >
                {/if}
            {/each}
        </div>
    {/if}
    <div class={view === "table" ? "table" : "hidden-text"}>
        <table>
            <caption>{label}, in {unit}</caption>
            <thead>
                <tr>
                    <th scope="col">Time</th>
                    {#each series as entry (entry.label)}<th scope="col">{entry.label}</th>{/each}
                </tr>
            </thead>
            <tbody>
                {#each times as time, row (time)}
                    <tr>
                        <th scope="row">{long.format(new Date(time * 1000))}</th>
                        {#each series as entry (entry.label)}<td
                                >{entry.values[row] === null ? "no reading" : number.format(entry.values[row] ?? 0)}</td
                            >{/each}
                    </tr>
                {/each}
            </tbody>
        </table>
    </div>
</figure>

<style>
    .time-series {
        display: flex;
        flex-direction: column;
        gap: 8px;
        margin: 0;
        color: var(--ambrose-color-fg-body);
        font-family: var(--ambrose-font-interface);
        container-type: inline-size;
    }

    .time-series.fill {
        flex: 1;
        min-height: 0;
    }

    .bar {
        display: flex;
        flex-wrap: wrap;
        align-items: center;
        gap: 8px;
    }

    .legend {
        display: flex;
        flex-wrap: wrap;
        gap: 4px;
    }

    .key,
    .view {
        display: inline-flex;
        align-items: center;
        gap: 6px;
        min-height: 28px;
        padding: 0 10px;
        border: 1px solid var(--ambrose-color-edge-quiet);
        border-radius: 6px;
        background: transparent;
        color: var(--ambrose-color-fg-muted);
        font: inherit;
        font-size: 12px;
        cursor: pointer;
    }

    .key[aria-pressed="true"] {
        color: var(--ambrose-color-fg-body);
    }

    .view[aria-pressed="true"] {
        border-color: var(--ambrose-color-edge-strong);
        background: var(--ambrose-color-surface-sunken);
        color: var(--ambrose-color-fg-body);
    }

    .key[aria-pressed="false"] .swatch {
        opacity: 0.35;
    }

    .view {
        margin-left: auto;
    }

    .key:hover,
    .view:hover {
        border-color: var(--ambrose-color-edge-strong);
        color: var(--ambrose-color-fg-body);
    }

    .key:focus-visible,
    .view:focus-visible {
        outline: 2px solid var(--ambrose-color-focus-ring);
        outline-offset: 2px;
    }

    .swatch {
        display: inline-block;
        width: 8px;
        height: 8px;
        border-radius: 2px;
    }

    .plot {
        position: relative;
        margin-left: 40px;
    }

    .plot.grow {
        flex: 1;
    }

    .holder,
    .marks {
        position: absolute;
        inset: 0;
    }

    .marks {
        pointer-events: none;
    }

    .guide {
        position: absolute;
        border-top: 1px dashed var(--ambrose-color-edge-strong);
    }

    .value {
        position: absolute;
        right: calc(100% + 8px);
        transform: translateY(-50%);
        color: var(--ambrose-color-fg-muted);
        font-family: var(--ambrose-font-mono);
        font-size: 11px;
        font-variant-numeric: tabular-nums;
        white-space: nowrap;
    }

    .crosshair {
        position: absolute;
        width: 1px;
        background: var(--ambrose-color-edge-strong);
    }

    .dot {
        position: absolute;
        width: 9px;
        height: 9px;
        border: 2px solid var(--ambrose-color-surface-card);
        border-radius: 50%;
        transform: translate(-50%, -50%);
    }

    .tip {
        position: absolute;
        top: 8px;
        z-index: 1;
        min-width: 120px;
        padding: 8px 10px;
        border: 1px solid var(--ambrose-color-edge-strong);
        border-radius: 8px;
        background: var(--ambrose-color-surface-card);
        font-size: 12px;
        transform: translateX(12px);
    }

    .tip.flip {
        transform: translateX(calc(-100% - 12px));
    }

    .when {
        margin-bottom: 4px;
        color: var(--ambrose-color-fg-muted);
        white-space: nowrap;
    }

    .row {
        display: flex;
        align-items: center;
        gap: 6px;
        white-space: nowrap;
    }

    .name {
        color: var(--ambrose-color-fg-muted);
    }

    .figure {
        margin-left: auto;
        font-family: var(--ambrose-font-mono);
        font-variant-numeric: tabular-nums;
    }

    .times {
        position: relative;
        height: 16px;
        margin-left: 40px;
        color: var(--ambrose-color-fg-muted);
        font-family: var(--ambrose-font-mono);
        font-size: 11px;
    }

    .time {
        position: absolute;
        transform: translateX(-50%);
        white-space: nowrap;
    }

    .time.first {
        transform: none;
    }

    .time.last {
        transform: translateX(-100%);
    }

    @container (width < 480px) {
        .time.minor {
            display: none;
        }
    }

    .table {
        max-height: 320px;
        overflow: auto;
        border: 1px solid var(--ambrose-color-edge-quiet);
        border-radius: 8px;
    }

    .table table {
        width: 100%;
        border-collapse: collapse;
        font-size: 12px;
    }

    .table caption {
        padding: 8px 10px;
        color: var(--ambrose-color-fg-muted);
        text-align: left;
    }

    .table th,
    .table td {
        padding: 4px 10px;
        border-top: 1px solid var(--ambrose-color-edge-quiet);
        text-align: left;
        font-weight: normal;
    }

    .table td {
        font-family: var(--ambrose-font-mono);
        font-variant-numeric: tabular-nums;
    }

    .table thead th {
        position: sticky;
        top: 0;
        background: var(--ambrose-color-surface-card);
        color: var(--ambrose-color-fg-muted);
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
