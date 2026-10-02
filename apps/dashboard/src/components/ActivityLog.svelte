<!-- Project Ambrose by Imjustchico: The panel's activity log for one scope, everything, one operator's own or one app's: filters by event, subject, result, time and address, newest first a hundred rows at a time behind the server's cursor, each row the server's sentence shown as text, who acted and whether a key or a schedule did, the address when this operator may see it, the time relative with the absolute time on hover, a details view of everything else the row holds, and export links for those allowed to export. -->
<script lang="ts">
    import * as Card from "$lib/components/ui/card/index.js";
    import * as Table from "$lib/components/ui/table/index.js";
    import { Button } from "$lib/components/ui/button/index.js";
    import { Input } from "$lib/components/ui/input/index.js";
    import { Label } from "$lib/components/ui/label/index.js";
    import { ApiError } from "$lib/api.svelte.js";
    import { formatAge } from "$lib/format.js";
    import { activityExportPath, panelActivity, type ActivityFilter, type ActivityScope } from "$lib/supervision.svelte.js";
    import type { PanelActivityRow } from "$lib/schemas.js";
    import StatusBadge from "./StatusBadge.svelte";

    let { scope, user = "" }: { scope: ActivityScope; user?: string } = $props();

    let prefix = $state("");
    let subject = $state("");
    let result = $state("");
    let address = $state("");
    let from = $state("");
    let to = $state("");
    let rows = $state<PanelActivityRow[]>([]);
    let cursor = $state<string | null>(null);
    let seesAddresses = $state(false);
    let canExport = $state(false);
    let loading = $state(false);
    let failure = $state("");
    let opened = $state<number | null>(null);
    let now = $state(Date.now());

    const filter = $derived<ActivityFilter>({
        prefix: prefix.trim(),
        user: user.trim(),
        subject: scope.kind === "app" ? "" : subject.trim(),
        result,
        address: address.trim(),
        from: from === "" ? undefined : new Date(from).getTime(),
        to: to === "" ? undefined : new Date(to).getTime(),
        limit: 100,
    });

    async function load(more: boolean, signal?: AbortSignal) {
        loading = true;
        try {
            const answer = await panelActivity(scope, { ...filter, cursor: more && cursor !== null ? cursor : undefined }, signal);
            rows = more ? [...rows, ...answer.rows] : answer.rows;
            cursor = answer.next_cursor;
            seesAddresses = answer.sees_addresses;
            canExport = answer.can_export;
            failure = "";
            now = Date.now();
        } catch (problem) {
            if (signal?.aborted) return;
            if (!more) rows = [];
            failure = problem instanceof ApiError ? problem.message : "The activity log could not be read";
        } finally {
            loading = false;
        }
    }

    $effect(() => {
        void filter;
        void scope;
        const controller = new AbortController();
        opened = null;
        void load(false, controller.signal);
        return () => controller.abort();
    });

    function absolute(epochMs: number): string {
        const date = new Date(epochMs);
        return Number.isNaN(date.getTime()) ? "Unknown time" : date.toLocaleString();
    }

    function tone(row: PanelActivityRow) {
        if (row.result === "ok") return "healthy" as const;
        if (row.result === "throttled") return "waiting" as const;
        return "wrong" as const;
    }

    const exportCsv = $derived(activityExportPath(scope, filter, "csv"));
    const exportJson = $derived(activityExportPath(scope, filter, "json"));
</script>

<Card.Root>
    <Card.Header class="gap-4">
        <div class="grid gap-3 sm:grid-cols-2 lg:grid-cols-3">
            <div class="grid gap-1">
                <Label for="activity-prefix">Event</Label>
                <Input id="activity-prefix" bind:value={prefix} placeholder="app:power or panel:session" />
            </div>
            {#if scope.kind !== "app"}
                <div class="grid gap-1">
                    <Label for="activity-subject">Subject</Label>
                    <Input id="activity-subject" bind:value={subject} placeholder="realm:1, node:main, game_account:42" />
                </div>
            {/if}
            <div class="grid gap-1">
                <Label for="activity-result">Result</Label>
                <select id="activity-result" bind:value={result} class="h-9 rounded-md border bg-background px-2 text-sm">
                    <option value="">Any result</option>
                    <option value="ok">Done</option>
                    <option value="denied">Denied</option>
                    <option value="failed">Failed</option>
                    <option value="throttled">Held back</option>
                </select>
            </div>
            <div class="grid gap-1">
                <Label for="activity-from">From</Label>
                <Input id="activity-from" type="datetime-local" bind:value={from} />
            </div>
            <div class="grid gap-1">
                <Label for="activity-to">To</Label>
                <Input id="activity-to" type="datetime-local" bind:value={to} />
            </div>
            {#if seesAddresses}
                <div class="grid gap-1">
                    <Label for="activity-address">Address</Label>
                    <Input id="activity-address" bind:value={address} placeholder="203.0.113.7" />
                </div>
            {/if}
        </div>
        {#if canExport}
            <div class="flex gap-2">
                <Button variant="outline" size="sm" href={exportCsv} download="ambrose-activity.csv">Export CSV</Button>
                <Button variant="outline" size="sm" href={exportJson} download="ambrose-activity.json">Export JSON</Button>
            </div>
        {/if}
    </Card.Header>
    <Card.Content>
        {#if failure !== ""}
            <p class="py-4 text-sm text-destructive">{failure}</p>
        {:else if rows.length === 0}
            <p class="py-6 text-sm text-muted-foreground">
                {loading ? "Reading the activity log." : "Nothing in the activity log matches that."}
            </p>
        {:else}
            <Table.Root>
                <Table.Header>
                    <Table.Row>
                        <Table.Head>When</Table.Head>
                        <Table.Head>What happened</Table.Head>
                        <Table.Head class="hidden md:table-cell">From</Table.Head>
                        <Table.Head>Result</Table.Head>
                        <Table.Head><span class="sr-only">Details</span></Table.Head>
                    </Table.Row>
                </Table.Header>
                <Table.Body>
                    {#each rows as row (row.id)}
                        <Table.Row>
                            <Table.Cell class="text-xs whitespace-nowrap" title={absolute(row.epoch_ms)}
                                >{formatAge(now - row.epoch_ms)} ago</Table.Cell
                            >
                            <Table.Cell>
                                <span class="activity-sentence">{row.sentence}</span>
                                {#if row.api_key_id}
                                    <StatusBadge tone="unknown" class="ml-2">API key {row.api_key_id}</StatusBadge>
                                {/if}
                                {#if row.scheduled}
                                    <StatusBadge tone="unknown" class="ml-2">Schedule</StatusBadge>
                                {/if}
                            </Table.Cell>
                            <Table.Cell class="hidden font-mono text-xs md:table-cell"
                                >{row.address ?? (row.address_shown ? "" : "Hidden")}</Table.Cell
                            >
                            <Table.Cell>
                                <StatusBadge tone={tone(row)}
                                    >{row.result === "ok"
                                        ? "Done"
                                        : row.result === "denied"
                                          ? "Denied"
                                          : row.result === "failed"
                                            ? "Failed"
                                            : "Held back"}</StatusBadge
                                >
                            </Table.Cell>
                            <Table.Cell>
                                <Button
                                    variant="ghost"
                                    size="sm"
                                    aria-expanded={opened === row.id}
                                    aria-label={`Details of event ${row.id}`}
                                    onclick={() => (opened = opened === row.id ? null : row.id)}>Details</Button
                                >
                            </Table.Cell>
                        </Table.Row>
                        {#if opened === row.id}
                            <Table.Row>
                                <Table.Cell colspan={5}>
                                    <dl class="grid gap-x-4 gap-y-1 text-sm sm:grid-cols-[max-content_1fr]">
                                        <dt class="text-muted-foreground">Event</dt>
                                        <dd class="font-mono">{row.name}</dd>
                                        <dt class="text-muted-foreground">When</dt>
                                        <dd>{absolute(row.epoch_ms)}</dd>
                                        <dt class="text-muted-foreground">Who</dt>
                                        <dd>{row.actor.name} ({row.actor.type})</dd>
                                        {#if row.reason}
                                            <dt class="text-muted-foreground">Reason</dt>
                                            <dd>{row.reason}</dd>
                                        {/if}
                                        {#if row.error}
                                            <dt class="text-muted-foreground">Error</dt>
                                            <dd>{row.error}</dd>
                                        {/if}
                                        {#if row.user_agent}
                                            <dt class="text-muted-foreground">Agent</dt>
                                            <dd>{row.user_agent}</dd>
                                        {/if}
                                        <dt class="text-muted-foreground">Subjects</dt>
                                        <dd>{row.subjects.map((item) => `${item.kind} ${item.name || item.id}`).join(", ") || "None"}</dd>
                                        <dt class="text-muted-foreground">Properties</dt>
                                        <dd><pre class="text-xs whitespace-pre-wrap">{JSON.stringify(row.properties, null, 2)}</pre></dd>
                                    </dl>
                                </Table.Cell>
                            </Table.Row>
                        {/if}
                    {/each}
                </Table.Body>
            </Table.Root>
            {#if cursor !== null}
                <div class="pt-4">
                    <Button variant="outline" size="sm" disabled={loading} onclick={() => load(true)}>Show older</Button>
                </div>
            {/if}
        {/if}
    </Card.Content>
</Card.Root>
