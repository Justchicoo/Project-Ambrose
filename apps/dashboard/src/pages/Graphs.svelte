<!-- Project Ambrose by Imjustchico: What every app has cost the machine over time, read from the supervisor's own history rather than from each app, because the supervisor goes on watching while an app is stopped and a stopped app is exactly what a graph has to be able to show. One app is chosen at a time and one range from five minutes to thirty days, and the server decides which resolution answers it, so a month reads no more points than an hour does. A stretch nothing was recorded in is a gap and is said to be one, never a zero, since a zero would read as an app that was running and doing nothing. Every series is shown as its latest reading with the lowest and highest of the range beside it, because a canvas is not something a screen reader can read and the numbers are the part that must always be there. -->
<script lang="ts">
    import { TimeSeries } from "@ambrose/ui";
    import * as Card from "$lib/components/ui/card/index.js";
    import * as Table from "$lib/components/ui/table/index.js";
    import * as Tabs from "$lib/components/ui/tabs/index.js";
    import * as ToggleGroup from "$lib/components/ui/toggle-group/index.js";
    import { ApiError } from "$lib/api.svelte.js";
    import { live } from "$lib/status.svelte.js";
    import { theme } from "$lib/theme.svelte.js";
    import { graphRange, graphs } from "$lib/supervision.svelte.js";
    import type { GraphRangeAnswer, GraphsAnswer } from "$lib/schemas.js";
    import PageHeader from "../components/PageHeader.svelte";
    import StatusBadge from "../components/StatusBadge.svelte";

    type Span = { label: string; milliseconds: number; points: number };

    const spans: Span[] = [
        { label: "5m", milliseconds: 5 * 60 * 1000, points: 120 },
        { label: "1h", milliseconds: 60 * 60 * 1000, points: 240 },
        { label: "6h", milliseconds: 6 * 60 * 60 * 1000, points: 360 },
        { label: "24h", milliseconds: 24 * 60 * 60 * 1000, points: 480 },
        { label: "7d", milliseconds: 7 * 24 * 60 * 60 * 1000, points: 480 },
        { label: "30d", milliseconds: 30 * 24 * 60 * 60 * 1000, points: 720 },
    ];

    let listing = $state<GraphsAnswer | null>(null);
    let ranges = $state<Record<string, GraphRangeAnswer>>({});
    let failure = $state("");
    let chosen = $state("");
    let spanLabel = $state("1h");

    const subjects = $derived(listing?.subjects ?? []);
    const subject = $derived(subjects.find((one) => one.subject === chosen) ?? subjects[0]);
    const span = $derived(spans.find((one) => one.label === spanLabel) ?? spans[1]);

    $effect(() => {
        const beat = live.now;
        void beat;
        const controller = new AbortController();
        void (async () => {
            try {
                listing = await graphs(controller.signal);
                failure = "";
            } catch (problem) {
                if (controller.signal.aborted) return;
                listing = null;
                failure = problem instanceof ApiError ? problem.message : "The history could not be read";
            }
        })();
        return () => controller.abort();
    });

    $effect(() => {
        const beat = live.now;
        const here = subject;
        const window = span;
        void beat;
        if (!here) return;
        const controller = new AbortController();
        void (async () => {
            const to = Date.now();
            const from = to - window.milliseconds;
            const read: Record<string, GraphRangeAnswer> = {};
            for (const name of here.series) {
                try {
                    read[name] = await graphRange(here.subject, name, from, to, window.points, controller.signal);
                } catch (problem) {
                    if (controller.signal.aborted) return;
                    void problem;
                }
            }
            if (!controller.signal.aborted) ranges = read;
        })();
        return () => controller.abort();
    });

    function readings(answer: GraphRangeAnswer | undefined): number[] {
        return (answer?.values ?? []).filter((value): value is number => value !== null);
    }

    function latest(answer: GraphRangeAnswer | undefined): number | null {
        const values = answer?.values ?? [];
        for (let at = values.length - 1; at >= 0; --at) {
            const value = values[at];
            if (value !== null && value !== undefined) return value;
        }
        return null;
    }

    function gaps(answer: GraphRangeAnswer | undefined): number {
        return (answer?.values ?? []).filter((value) => value === null).length;
    }

    function round(value: number, places: number): string {
        const text = value.toFixed(places);
        if (!text.includes(".")) return text;
        return text.replace(/\.?0+$/, "") || "0";
    }

    function bytes(value: number): string {
        const units = ["B", "KiB", "MiB", "GiB", "TiB"];
        let at = 0;
        let left = value;
        while (left >= 1024 && at < units.length - 1) {
            left /= 1024;
            ++at;
        }
        return `${round(left, at === 0 ? 0 : 1)} ${units[at]}`;
    }

    function spell(series: string, value: number | null): string {
        if (value === null) return "no reading";
        if (series.endsWith("_bytes")) return bytes(value);
        if (series.endsWith("_percent")) return `${round(value, 1)}%`;
        return round(value, 0);
    }

    function unitOf(series: string): string {
        if (series.endsWith("_bytes")) return "bytes";
        if (series.endsWith("_percent")) return "percent";
        return "count";
    }

    function title(series: string): string {
        const words: Record<string, string> = {
            cpu_percent: "Processor",
            memory_bytes: "Memory",
            threads: "Threads",
            handles: "Open handles",
            disk_read_bytes: "Read from disk",
            disk_write_bytes: "Written to disk",
        };
        return words[series] ?? series;
    }
</script>

<PageHeader title="Resources" description="What each server has cost the machine, as far back as the supervisor has watched." />

{#if failure !== ""}
    <Card.Root class="mb-4">
        <Card.Content class="py-4 text-sm text-destructive">{failure}</Card.Content>
    </Card.Root>
{/if}

{#if subjects.length === 0}
    <Card.Root>
        <Card.Content class="py-6 text-sm text-muted-foreground">
            {listing === null
                ? "Reading the history."
                : "Nothing has been recorded yet. The supervisor writes a reading for every app it runs, a few seconds apart."}
        </Card.Content>
    </Card.Root>
{:else}
    <Tabs.Root bind:value={chosen} class="gap-4">
        <Tabs.List aria-label="Server whose resources to show">
            {#each subjects as entry (entry.subject)}
                <Tabs.Trigger value={entry.subject} class="font-mono text-xs">{entry.subject}</Tabs.Trigger>
            {/each}
        </Tabs.List>

        <ToggleGroup.Root type="single" variant="outline" size="sm" spacing={1} bind:value={spanLabel} aria-label="How far back to show">
            {#each spans as one (one.label)}
                <ToggleGroup.Item value={one.label} class="px-2.5">{one.label}</ToggleGroup.Item>
            {/each}
        </ToggleGroup.Root>

        <Card.Root>
            <Card.Header>
                <Card.Title>{subject?.subject ?? ""} over the last {span.label}</Card.Title>
                <Card.Description>
                    The server chooses the resolution that answers the range, so thirty days reads no more points than an hour does. A gap
                    is a stretch the supervisor recorded nothing in, which is an app that was not running.
                </Card.Description>
            </Card.Header>
            <Card.Content class="flex flex-col gap-6">
                {#each subject?.series ?? [] as name (name)}
                    {@const answer = ranges[name]}
                    {#if answer && answer.at_epoch_ms.length > 1}
                        <TimeSeries
                            label={`${title(name)} of ${subject?.subject ?? ""}`}
                            unit={unitOf(name)}
                            times={answer.at_epoch_ms}
                            series={[{ label: title(name), values: answer.values }]}
                            theme={theme.resolved}
                            height={180}
                        />
                    {/if}
                {/each}
                <Table.Root>
                    <Table.Header>
                        <Table.Row>
                            <Table.Head>Series</Table.Head>
                            <Table.Head class="text-right">Latest</Table.Head>
                            <Table.Head class="text-right">Lowest</Table.Head>
                            <Table.Head class="text-right">Highest</Table.Head>
                            <Table.Head class="text-right">Recorded</Table.Head>
                        </Table.Row>
                    </Table.Header>
                    <Table.Body>
                        {#each subject?.series ?? [] as name (name)}
                            {@const answer = ranges[name]}
                            {@const values = readings(answer)}
                            <Table.Row>
                                <Table.Cell class="font-medium">{title(name)}</Table.Cell>
                                <Table.Cell class="text-right tabular-nums">{spell(name, latest(answer))}</Table.Cell>
                                <Table.Cell class="text-right tabular-nums">
                                    {values.length === 0 ? "—" : spell(name, Math.min(...values))}
                                </Table.Cell>
                                <Table.Cell class="text-right tabular-nums">
                                    {values.length === 0 ? "—" : spell(name, Math.max(...values))}
                                </Table.Cell>
                                <Table.Cell class="text-right">
                                    {#if answer === undefined}
                                        <span class="text-muted-foreground">reading</span>
                                    {:else if values.length === 0}
                                        <StatusBadge tone="wrong">nothing in this range</StatusBadge>
                                    {:else if gaps(answer) > 0}
                                        <StatusBadge tone="waiting">{values.length} of {answer.points}</StatusBadge>
                                    {:else}
                                        <StatusBadge tone="healthy">{values.length} of {answer.points}</StatusBadge>
                                    {/if}
                                </Table.Cell>
                            </Table.Row>
                        {/each}
                    </Table.Body>
                </Table.Root>
            </Card.Content>
        </Card.Root>
    </Tabs.Root>
{/if}
