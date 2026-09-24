<!-- Project Ambrose by Imjustchico: The overview of the app that served the panel, read live, and, when the supervisor served it, a card for every app it runs with the state, uptime and crashes the supervisor reports: headline figures and a card with its state, place, uptime, build, sessions, memory, threads and tick times, badges and problems with the button that fixes each where the build reports the problem, every figure carrying the age of its sample and demoted once it goes stale, and the last fifteen minutes of what it reports over time. -->
<script lang="ts">
    import { Sparkline, TimeSeries } from "@ambrose/ui";
    import * as Card from "$lib/components/ui/card/index.js";
    import { Button } from "$lib/components/ui/button/index.js";
    import { formatAge, formatBytes, formatUptime } from "$lib/format.js";
    import { history, isStale, live } from "$lib/status.svelte.js";
    import { supervised, supervisorServes } from "$lib/supervision.svelte.js";
    import { theme } from "$lib/theme.svelte.js";
    import type { Problem } from "$lib/schemas.js";
    import CircleAlertIcon from "@lucide/svelte/icons/circle-alert";
    import PageHeader from "../components/PageHeader.svelte";
    import StatusBadge from "../components/StatusBadge.svelte";

    type Tone = "healthy" | "waiting" | "wrong" | "unknown";

    const fixes: Record<string, { route: string; label: string }> = {
        install_missing: { route: "client", label: "Open client data" },
        type_dump_missing: { route: "client", label: "Open client data" },
        type_dump_stale: { route: "client", label: "Open client data" },
        revision_not_allowed: { route: "client", label: "Open client data" },
        database_unreachable: { route: "settings", label: "Open settings" },
        schema_update_pending: { route: "servers", label: "Open servers" },
    };

    const status = $derived(live.status);
    const app = $derived(live.apps[0]);
    const watched = $derived(supervised());
    const runs = $derived(supervisorServes());
    const running = $derived(watched.filter((entry) => entry.supervision?.state === "running").length);
    const crashed = $derived(watched.filter((entry) => entry.supervision?.state === "crashed").length);
    const stale = $derived(isStale(live.receivedAt, live.now));
    const reachable = $derived(live.connection === "live");
    const age = $derived(live.receivedAt === 0 ? "" : formatAge(live.now - live.receivedAt));
    const known = $derived(new Set((live.capabilities?.problem_codes ?? []).map((entry) => entry.code)));
    const problems = $derived(status?.problems ?? []);

    const state = $derived.by((): { tone: Tone; word: string } => {
        if (!status || !reachable) return { tone: "unknown", word: live.connection === "disconnected" ? "Signed out" : "Not answering" };
        if (status.state === "running")
            return problems.length > 0 ? { tone: "waiting", word: "Needs attention" } : { tone: "healthy", word: "Running" };
        if (status.state === "starting" || status.state === "stopping")
            return { tone: "waiting", word: status.state === "starting" ? "Starting" : "Stopping" };
        return { tone: "unknown", word: status.state };
    });

    const place = $derived(!app ? "" : app.address === "" || app.port === 0 ? "No client listener" : `${app.address}:${app.port}`);
    const figures = $derived([
        { label: "State", value: state.word, number: false, detail: app ? `${app.role}, ${place.toLowerCase()}` : "Reading the app list" },
        {
            label: "Uptime",
            value: status ? formatUptime(status.uptime) : "Not read yet",
            number: status !== null,
            detail: status ? `Build ${status.revision}` : "",
        },
        runs
            ? {
                  label: "Apps running",
                  value: `${running} of ${watched.length}`,
                  number: true,
                  detail: crashed > 0 ? `${crashed} crashed` : "Every app the supervisor runs",
              }
            : {
                  label: "Sessions",
                  value: typeof status?.sessions === "number" ? String(status.sessions) : "Not reported",
                  number: typeof status?.sessions === "number",
                  detail: "Clients connected to this app",
              },
        {
            label: "Open problems",
            value: status ? String(problems.length) : "Not read yet",
            number: status !== null,
            detail: problems[0]?.message ?? "Nothing needs fixing",
        },
    ]);

    const charted = $derived.by(() => {
        void live.drawn;
        const average = history.tickAverage.read();
        if (status?.tick) {
            return {
                title: "Tick time, last 15 minutes",
                unit: "ms",
                times: average.times,
                series: [
                    { label: "Average tick", values: average.values },
                    { label: "Worst tick", values: history.tickMax.read().values },
                ],
            };
        }
        const sessions = history.sessions.read();
        if (status?.sessions != null)
            return {
                title: "Sessions, last 15 minutes",
                unit: "sessions",
                times: sessions.times,
                series: [{ label: "Sessions", values: sessions.values }],
            };
        return null;
    });

    const spark = $derived.by(() => {
        void live.drawn;
        const ring = status?.tick ? history.tickAverage.read() : history.sessions.read();
        return {
            label: status?.tick ? "Average tick time over the last minutes" : "Sessions over the last minutes",
            read: ring.values.some((value) => value !== null),
            ...ring,
        };
    });

    function fixFor(problem: Problem) {
        return known.has(problem.code) ? fixes[problem.code] : undefined;
    }
</script>

<PageHeader
    title="Overview"
    description={app ? `${app.name}, live from the server that served this panel.` : "The server that served this panel, live."}
/>

<div class="grid gap-4 @xl/main:grid-cols-2 @5xl/main:grid-cols-4">
    {#each figures as figure (figure.label)}
        <Card.Root class="@container/card gap-4 bg-gradient-to-t from-primary/5 to-card shadow-xs">
            <Card.Header>
                <Card.Description>{figure.label}</Card.Description>
                <Card.Title
                    class={`text-2xl font-semibold @[250px]/card:text-3xl ${figure.number ? "font-mono tabular-nums" : ""} ${stale ? "text-muted-foreground" : ""}`}
                    >{figure.value}</Card.Title
                >
            </Card.Header>
            <Card.Footer class="flex-col items-start gap-1 text-sm">
                <div class="line-clamp-1 font-medium">{figure.detail}</div>
                <div class={stale ? "text-unknown" : "text-muted-foreground"}>
                    {age === "" ? "No sample yet" : stale ? `Last sample ${age} ago` : `Updated ${age} ago`}
                </div>
            </Card.Footer>
        </Card.Root>
    {/each}
</div>

{#if runs}
    <div class="grid gap-4 @xl/main:grid-cols-2 @5xl/main:grid-cols-3">
        {#each watched as entry (entry.name)}
            {@const supervision = entry.supervision}
            {@const alive = supervision?.state === "running"}
            <Card.Root class="gap-3 shadow-xs">
                <Card.Header>
                    <Card.Title class="font-serif text-lg">{entry.name}</Card.Title>
                    <Card.Description class="text-xs">
                        {entry.role}{entry.realm ? ` · ${entry.realm}` : ""}{supervision?.pid ? ` · process ${supervision.pid}` : ""}
                    </Card.Description>
                    <Card.Action>
                        <StatusBadge
                            tone={alive
                                ? "healthy"
                                : supervision?.state === "crashed"
                                  ? "wrong"
                                  : supervision?.state === "offline"
                                    ? "unknown"
                                    : "waiting"}
                            pulse={alive}
                        >
                            {(supervision?.state ?? "unknown").charAt(0).toUpperCase() + (supervision?.state ?? "unknown").slice(1)}
                        </StatusBadge>
                    </Card.Action>
                </Card.Header>
                <Card.Content class="space-y-1 text-sm">
                    <div class="flex justify-between">
                        <span class="text-muted-foreground">Uptime</span>
                        <span class="font-medium tabular-nums">
                            {alive && supervision?.started_epoch_ms
                                ? formatUptime(Math.max(0, Math.round((live.now - supervision.started_epoch_ms) / 1000)))
                                : "Not running"}
                        </span>
                    </div>
                    <div class="flex justify-between">
                        <span class="text-muted-foreground">Crashes</span>
                        <span class="font-medium tabular-nums">{supervision?.crashes ?? 0}</span>
                    </div>
                    {#if supervision?.message}
                        <p class="text-xs text-muted-foreground">{supervision.message}</p>
                    {/if}
                </Card.Content>
                <Card.Footer>
                    <Button size="sm" variant="outline" href="#servers">Open servers</Button>
                </Card.Footer>
            </Card.Root>
        {/each}
    </div>
{/if}

<div class="grid gap-4 @5xl/main:grid-cols-3">
    <Card.Root class="shadow-xs">
        <Card.Header>
            <Card.Title class="flex items-center gap-2 font-serif text-xl">
                {app?.name ?? status?.app ?? "This app"}
                {#if app}<span class="rounded-md border px-1.5 py-0.5 font-sans text-xs font-normal text-muted-foreground">{app.role}</span
                    >{/if}
            </Card.Title>
            <Card.Description class={place.includes(":") ? "font-mono text-xs" : "text-xs"}>{place}</Card.Description>
            <Card.Action><StatusBadge tone={state.tone} pulse={state.tone === "healthy" && !stale}>{state.word}</StatusBadge></Card.Action>
        </Card.Header>
        <Card.Content class={`flex-1 space-y-4 ${stale ? "text-muted-foreground" : ""}`}>
            {#if app?.realm || problems.length > 0}
                <div class="flex flex-wrap gap-1.5">
                    {#if app?.realm}<StatusBadge tone="mine">{app.realm}</StatusBadge>{/if}
                    {#if problems.some((problem) => problem.code === "schema_update_pending")}<StatusBadge tone="waiting"
                            >Pending SQL updates</StatusBadge
                        >{/if}
                    {#if problems.some((problem) => problem.code === "revision_not_allowed")}<StatusBadge tone="waiting"
                            >Client revision not allowed</StatusBadge
                        >{/if}
                    {#if problems.length > 0}<StatusBadge tone="wrong"
                            >{problems.length} problem{problems.length === 1 ? "" : "s"}</StatusBadge
                        >{/if}
                </div>
            {/if}
            {#if status}
                <dl class="grid grid-cols-2 gap-x-4 gap-y-3 text-sm">
                    <div>
                        <dt class="text-xs text-muted-foreground">Uptime</dt>
                        <dd class="font-medium">{formatUptime(status.uptime)}</dd>
                    </div>
                    <div>
                        <dt class="text-xs text-muted-foreground">Build</dt>
                        <dd class="font-mono font-medium">{status.revision}</dd>
                    </div>
                    <div>
                        <dt class="text-xs text-muted-foreground">Sessions</dt>
                        <dd class="font-medium tabular-nums">{status.sessions ?? "Not reported"}</dd>
                    </div>
                    <div>
                        <dt class="text-xs text-muted-foreground">Memory</dt>
                        <dd class="font-medium tabular-nums">
                            {status.memory ? formatBytes(status.memory.resident_bytes) : "Not reported"}
                        </dd>
                    </div>
                    <div>
                        <dt class="text-xs text-muted-foreground">Threads</dt>
                        <dd class="font-medium tabular-nums">{status.threads ?? "Not reported"}</dd>
                    </div>
                    {#if status.tick}
                        <div>
                            <dt class="text-xs text-muted-foreground">Tick, average and worst</dt>
                            <dd class="font-medium tabular-nums">
                                {status.tick.average_ms.toFixed(1)} ms / {status.tick.max_ms.toFixed(1)} ms
                            </dd>
                        </div>
                    {/if}
                </dl>
                {#if status.role === "game"}
                    <p class="text-xs text-muted-foreground">Players against the realm's limit arrive with milestone 4.01.</p>
                {/if}
                {#if spark.read && spark.times.length > 1}
                    <Sparkline
                        label={spark.label}
                        times={spark.times}
                        values={spark.values}
                        unit={status.tick ? "ms" : "sessions"}
                        theme={theme.resolved}
                        height={56}
                    />
                    <p class="text-xs text-muted-foreground">{spark.label}</p>
                {/if}
                {#each problems as problem, index (index)}
                    {@const fix = fixFor(problem)}
                    <div class="flex items-start gap-3 rounded-lg border border-destructive/30 bg-destructive/5 p-3 text-foreground">
                        <CircleAlertIcon class="mt-0.5 size-4 shrink-0 text-destructive" />
                        <div class="flex-1 space-y-2">
                            <p class="text-sm">{problem.message}</p>
                            {#if fix}<Button size="sm" variant="outline" href={`#${fix.route}`}>{fix.label}</Button>{/if}
                        </div>
                    </div>
                {/each}
            {:else}
                <p class="text-sm text-muted-foreground">Reading this app's status.</p>
            {/if}
        </Card.Content>
        <Card.Footer>
            <span class={`text-xs ${stale ? "text-unknown" : "text-muted-foreground"}`}
                >{age === "" ? "No sample yet" : stale ? `Last sample ${age} ago` : `Updated ${age} ago`}</span
            >
        </Card.Footer>
    </Card.Root>

    <Card.Root class="shadow-xs @5xl/main:col-span-2">
        <Card.Header>
            <Card.Title>{charted?.title ?? "Over time"}</Card.Title>
            <Card.Description>Samples this panel has read since it opened, one a second</Card.Description>
        </Card.Header>
        <Card.Content class="flex flex-1 flex-col">
            {#if charted && charted.times.length > 1}
                <TimeSeries
                    label={charted.title}
                    unit={charted.unit}
                    times={charted.times}
                    series={charted.series}
                    theme={theme.resolved}
                    height={200}
                    fill
                />
            {:else if charted}
                <p class="text-sm text-muted-foreground">The chart starts once a second sample arrives.</p>
            {:else}
                <p class="text-sm text-muted-foreground">
                    This app reports nothing over time yet. Resource graphs for every app arrive with milestone 17.19.
                </p>
            {/if}
        </Card.Content>
    </Card.Root>
</div>
