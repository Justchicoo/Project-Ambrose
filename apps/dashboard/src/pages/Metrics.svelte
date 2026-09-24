<!-- Project Ambrose by Imjustchico: What the chosen server is counting right now, read live from its own register: every metric with the line of help it carries, and under it one row per label set, so an operator can see the work each pool, service and reload target is doing without standing up a scraper first. A histogram is shown as the count of observations, their total and the average, and as the share that finished inside each bound, because the bounds are what say whether the slow ones are rare or usual. Graphs over time belong to 17.19, which keeps the history this page only ever shows the latest of. -->
<script lang="ts">
    import * as Card from "$lib/components/ui/card/index.js";
    import * as Table from "$lib/components/ui/table/index.js";
    import { ApiError } from "$lib/api.svelte.js";
    import { live } from "$lib/status.svelte.js";
    import { metricsOf, servedBy, supervised, supervisorServes } from "$lib/supervision.svelte.js";
    import type { MetricFamily, MetricSeries, MetricsAnswer } from "$lib/schemas.js";
    import PageHeader from "../components/PageHeader.svelte";
    import StatusBadge from "../components/StatusBadge.svelte";

    const choices = $derived(supervisorServes() ? [servedBy(), ...supervised().map((app) => app.name)] : [servedBy()]);
    let chosen = $state("");
    let answer = $state<MetricsAnswer | null>(null);
    let failure = $state("");

    const app = $derived(choices.includes(chosen) ? chosen : (choices[0] ?? ""));

    $effect(() => {
        const name = app;
        const beat = live.now;
        if (name === "") return;
        void beat;
        const controller = new AbortController();
        void (async () => {
            try {
                answer = await metricsOf(name, controller.signal);
                failure = "";
            } catch (problem) {
                if (controller.signal.aborted) return;
                answer = null;
                failure = problem instanceof ApiError ? problem.message : "The metrics could not be read";
            }
        })();
        return () => controller.abort();
    });

    const families = $derived(answer?.metrics ?? []);
    const seriesCount = $derived(families.reduce((total, family) => total + family.series.length, 0));

    function describe(series: MetricSeries): string {
        const names = Object.keys(series.labels);
        if (names.length === 0) return "all";
        return names
            .sort()
            .map((name) => `${name}=${series.labels[name]}`)
            .join(", ");
    }

    function round(value: number, places: number): string {
        const text = value.toFixed(places);
        if (!text.includes(".")) return text;
        return text.replace(/\.?0+$/, "") || "0";
    }

    function spell(seconds: number): string {
        if (seconds === 0) return "0";
        if (seconds < 0.001) return `${round(seconds * 1000000, 0)}µs`;
        if (seconds < 1) return `${round(seconds * 1000, 2)}ms`;
        return `${round(seconds, 3)}s`;
    }

    function average(series: MetricSeries): string {
        return series.count === 0 ? "no observations yet" : spell(series.sum / series.count);
    }

    function spread(series: MetricSeries): { le: number; share: number }[] {
        if (series.count === 0) return [];
        return series.buckets
            .filter((bucket) => bucket.count > 0)
            .map((bucket) => ({ le: bucket.le, share: (bucket.count / series.count) * 100 }));
    }

    function tone(kind: string): "healthy" | "waiting" | "wrong" {
        return kind === "counter" ? "healthy" : kind === "gauge" ? "waiting" : "wrong";
    }

    function reading(family: MetricFamily, series: MetricSeries): string {
        return family.kind === "histogram" ? `${series.count}` : `${series.value}`;
    }
</script>

<PageHeader title="Metrics" description="What this server is counting, straight from its own register." />

{#if failure !== ""}
    <Card.Root class="mb-4">
        <Card.Content class="py-4 text-sm text-destructive">{failure}</Card.Content>
    </Card.Root>
{/if}

<Card.Root class="mb-4">
    <Card.Header>
        <Card.Title>{families.length} metric(s), {seriesCount} series</Card.Title>
        <Card.Description>
            The same numbers a scraper reads from this server's /metrics, which is where graphs over time will come from.
        </Card.Description>
    </Card.Header>
</Card.Root>

{#if families.length === 0}
    <Card.Root>
        <Card.Content class="py-6 text-sm text-muted-foreground">
            {answer === null ? "Reading the metrics." : "This server has registered nothing yet."}
        </Card.Content>
    </Card.Root>
{:else}
    {#each families as family (family.name)}
        <Card.Root class="mb-4">
            <Card.Header>
                <Card.Title class="flex items-center gap-2">
                    <span class="font-mono text-sm">{family.name}</span>
                    <StatusBadge tone={tone(family.kind)}>{family.kind}</StatusBadge>
                </Card.Title>
                {#if family.help !== ""}
                    <Card.Description>{family.help}</Card.Description>
                {/if}
            </Card.Header>
            <Card.Content>
                <Table.Root>
                    <Table.Header>
                        <Table.Row>
                            <Table.Head>Series</Table.Head>
                            <Table.Head class="text-right">{family.kind === "histogram" ? "Observations" : "Reading"}</Table.Head>
                            {#if family.kind === "histogram"}
                                <Table.Head class="text-right">Total</Table.Head>
                                <Table.Head class="text-right">Average</Table.Head>
                                <Table.Head class="hidden lg:table-cell">Finished within</Table.Head>
                            {/if}
                        </Table.Row>
                    </Table.Header>
                    <Table.Body>
                        {#each family.series as series (describe(series))}
                            <Table.Row>
                                <Table.Cell class="font-mono text-xs">{describe(series)}</Table.Cell>
                                <Table.Cell class="text-right tabular-nums">{reading(family, series)}</Table.Cell>
                                {#if family.kind === "histogram"}
                                    <Table.Cell class="text-right tabular-nums">{spell(series.sum)}</Table.Cell>
                                    <Table.Cell class="text-right tabular-nums">{average(series)}</Table.Cell>
                                    <Table.Cell class="hidden lg:table-cell">
                                        {#if spread(series).length === 0}
                                            <span class="text-muted-foreground">nothing measured yet</span>
                                        {:else}
                                            <span class="font-mono text-xs">
                                                {spread(series)
                                                    .slice(0, 4)
                                                    .map((bucket) => `${round(bucket.share, 0)}% ≤ ${spell(bucket.le)}`)
                                                    .join(" · ")}
                                            </span>
                                        {/if}
                                    </Table.Cell>
                                {/if}
                            </Table.Row>
                        {/each}
                    </Table.Body>
                </Table.Root>
            </Card.Content>
        </Card.Root>
    {/each}
{/if}
