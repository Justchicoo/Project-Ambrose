/*
 * Project Ambrose by Imjustchico
 * The one place that touches uPlot's API: series drawn into a holder with no axes, legend or animation, gaps where a reading is missing, refilled and resized in place, and the scale positions and cursor that markup drawn over the canvas reads.
 */

import uPlot from "uplot";
import "uplot/dist/uPlot.min.css";

export type PlotSeries = {
    values: (number | null)[];
    color: string;
    fill: boolean;
    shown: boolean;
};

export type PlotSpec = {
    width: number;
    height: number;
    times: number[];
    series: PlotSeries[];
    top: number | null;
    time: boolean;
    cursor: ((index: number | null) => void) | null;
    drawn: (() => void) | null;
};

export type PlotArea = { left: number; top: number; width: number; height: number };

export type Plot = {
    update(times: number[], series: PlotSeries[], top: number | null): void;
    resize(width: number, height: number): void;
    show(index: number, shown: boolean): void;
    x(value: number): number;
    y(value: number): number;
    area(): PlotArea;
    destroy(): void;
};

function aligned(times: number[], series: PlotSeries[]): uPlot.AlignedData {
    return [times, ...series.map((entry) => entry.values)];
}

function seriesOptions(entry: PlotSeries): uPlot.Series {
    return {
        stroke: entry.color,
        fill: entry.fill ? `${entry.color}2e` : undefined,
        width: 1.5,
        spanGaps: false,
        show: entry.shown,
        points: { show: false },
    };
}

export function mountPlot(holder: HTMLElement, spec: PlotSpec): Plot {
    let top = spec.top;
    const onCursor = spec.cursor;
    const onDrawn = spec.drawn;
    const chart = new uPlot(
        {
            width: spec.width,
            height: spec.height,
            padding: [6, 2, 2, 2],
            legend: { show: false },
            select: { show: false, left: 0, top: 0, width: 0, height: 0 },
            cursor: onCursor
                ? { show: true, x: false, y: false, points: { show: false }, drag: { setScale: false, x: false, y: false } }
                : { show: false },
            axes: [{ show: false }, { show: false }],
            scales: {
                x: { time: spec.time },
                y: { range: (_chart, low, high) => (top === null ? [Math.min(0, low), Math.max(1, high)] : [0, top]) },
            },
            series: [{}, ...spec.series.map(seriesOptions)],
            hooks: {
                setCursor: onCursor ? [(plotted) => onCursor(plotted.cursor.idx ?? null)] : [],
                draw: onDrawn ? [() => onDrawn()] : [],
            },
        },
        aligned(spec.times, spec.series),
        holder,
    );

    return {
        update(times, series, nextTop) {
            top = nextTop;
            series.forEach((entry, index) => {
                if (chart.series[index + 1].show !== entry.shown) {
                    chart.setSeries(index + 1, { show: entry.shown });
                }
            });
            chart.setData(aligned(times, series));
        },
        resize(width, height) {
            chart.setSize({ width, height });
        },
        show(index, shown) {
            chart.setSeries(index + 1, { show: shown });
        },
        x(value) {
            return chart.over.offsetLeft + chart.valToPos(value, "x");
        },
        y(value) {
            return chart.over.offsetTop + chart.valToPos(value, "y");
        },
        area() {
            return {
                left: chart.over.offsetLeft,
                top: chart.over.offsetTop,
                width: chart.over.clientWidth,
                height: chart.over.clientHeight,
            };
        },
        destroy() {
            chart.destroy();
        },
    };
}
