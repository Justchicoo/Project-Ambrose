<!-- Project Ambrose by Imjustchico: Values over time with a crosshair, a legend and a table view: one series, two series with a stretch where the app was stopped, and the table view on its own. -->
<script module lang="ts">
    import { defineMeta } from "@storybook/addon-svelte-csf";
    import { expect, userEvent, within } from "storybook/test";
    import TimeSeries from "./TimeSeries.svelte";

    const { Story } = defineMeta({
        title: "Data/TimeSeries",
        component: TimeSeries,
        tags: ["autodocs"],
    });

    const start = Date.UTC(2026, 8, 22, 8, 0) / 1000;
    const times = Array.from({ length: 60 }, (_value, index) => start + index * 60);
    const players = times.map((_time, index) => 20 + Math.round(8 * Math.sin(index / 7)));
    const average = times.map((_time, index) =>
        index > 24 && index < 33 ? null : 3 + Math.round(10 * Math.abs(Math.sin(index / 9))) / 10,
    );
    const worst = average.map((value, index) => (value === null ? null : value + (index % 11 === 0 ? 8 : 2)));
</script>

<Story name="One series">
    {#snippet template()}
        <div class="w-full rounded-card bg-surface-card p-16">
            <TimeSeries label="Players online over the last hour" unit="players" {times} series={[{ label: "Players", values: players }]} />
        </div>
    {/snippet}
</Story>

<Story
    name="Two series with a stopped stretch"
    play={async ({ canvasElement }) => {
        const canvas = within(canvasElement);
        const worstKey = canvas.getByRole("button", { name: "Worst tick" });
        await userEvent.click(worstKey);
        await expect(worstKey).toHaveAttribute("aria-pressed", "false");
        await userEvent.click(worstKey);
        await expect(worstKey).toHaveAttribute("aria-pressed", "true");
    }}
>
    {#snippet template()}
        <div class="w-full rounded-card bg-surface-card p-16">
            <TimeSeries
                label="Tick time over the last hour"
                unit="ms"
                {times}
                series={[
                    { label: "Average tick", values: average },
                    { label: "Worst tick", values: worst },
                ]}
            />
        </div>
    {/snippet}
</Story>

<Story
    name="Table view"
    play={async ({ canvasElement }) => {
        const canvas = within(canvasElement);
        await userEvent.click(canvas.getByRole("button", { name: "Table view" }));
        await expect(canvas.getByRole("button", { name: "Table view" })).toHaveAttribute("aria-pressed", "true");
        await expect(canvas.getAllByText("no reading").length).toBeGreaterThan(0);
    }}
>
    {#snippet template()}
        <div class="w-full rounded-card bg-surface-card p-16">
            <TimeSeries
                label="Average tick time over the last hour"
                unit="ms"
                {times}
                series={[{ label: "Average tick", values: average }]}
            />
        </div>
    {/snippet}
</Story>
