<!-- Project Ambrose by Imjustchico: The live app overview and its world tick graph, named subsystem budget breakdown and bounded on-demand Chrome trace capture. -->
<script lang="ts">
    import { Sparkline, TimeSeries } from "@ambrose/ui";
    import * as Card from "$lib/components/ui/card/index.js";
    import { Button } from "$lib/components/ui/button/index.js";
    import { formatAge, formatBytes, formatUptime } from "$lib/format.js";
    import { history, isStale, live } from "$lib/status.svelte.js";
    import {
        metricsOf,
        startTickProfile,
        supervised,
        supervisorServes,
        tickProfileOf,
        tickProfileTraceOf,
    } from "$lib/supervision.svelte.js";
    import { theme } from "$lib/theme.svelte.js";
    import { ApiError } from "$lib/api.svelte.js";
    import type { MetricFamily, MetricsAnswer, Problem } from "$lib/schemas.js";
    import CircleAlertIcon from "@lucide/svelte/icons/circle-alert";
    import PageHeader from "../components/PageHeader.svelte";
    import MaintenanceCard from "../components/MaintenanceCard.svelte";
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
    const gameApp = $derived.by(() =>
        live.status?.role === "game" ? live.status.app : (live.apps.find((entry) => entry.role === "game")?.name ?? ""),
    );
    const watched = $derived(supervised());
    const runs = $derived(supervisorServes());
    const running = $derived(watched.filter((entry) => entry.supervision?.state === "running").length);
    const crashed = $derived(
        watched.filter((entry) => entry.supervision?.state === "crashed" || entry.supervision?.state === "backoff").length,
    );
    const stale = $derived(isStale(live.receivedAt, live.now));
    const reachable = $derived(live.connection === "live");
    const age = $derived(live.receivedAt === 0 ? "" : formatAge(live.now - live.receivedAt));
    const known = $derived(new Set((live.capabilities?.problem_codes ?? []).map((entry) => entry.code)));
    const problems = $derived(status?.problems ?? []);
    let tickMetrics = $state<MetricsAnswer | null>(null);
    let tickMetricsFailure = $state("");
    let captureSeconds = $state(5);
    let captureActive = $state(false);
    let captureMessage = $state("");

    $effect(() => {
        const target = gameApp;
        const beat = live.now;
        void beat;
        if (target === "") return;
        const controller = new AbortController();
        void (async () => {
            try {
                tickMetrics = await metricsOf(target, controller.signal);
                tickMetricsFailure = "";
            } catch (problem) {
                if (controller.signal.aborted) return;
                tickMetrics = null;
                tickMetricsFailure = problem instanceof ApiError ? problem.message : "The tick breakdown could not be read";
            }
        })();
        return () => controller.abort();
    });

    const overallState = $derived.by((): { tone: Tone; word: string } => {
        if (!status || !reachable) return { tone: "unknown", word: live.connection === "disconnected" ? "Signed out" : "Not answering" };
        if (status.state === "running")
            return problems.length > 0 ? { tone: "waiting", word: "Needs attention" } : { tone: "healthy", word: "Running" };
        if (status.state === "starting" || status.state === "stopping")
            return { tone: "waiting", word: status.state === "starting" ? "Starting" : "Stopping" };
        return { tone: "unknown", word: status.state };
    });

    const place = $derived(!app ? "" : app.address === "" || app.port === 0 ? "No client listener" : `${app.address}:${app.port}`);
    const figures = $derived([
        {
            label: "State",
            value: overallState.word,
            number: false,
            detail: app ? `${app.role}, ${place.toLowerCase()}` : "Reading the app list",
        },
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

    function componentValue(name: string, family: MetricFamily | undefined): number | null {
        if (!family) return null;
        const found = family.series.find((one) => one.labels.component === name);
        return found?.value ?? null;
    }

    const tickComponents = $derived.by(() => {
        const elapsed = tickMetrics?.metrics.find((family) => family.name === "ambrose_world_tick_subsystem_nanoseconds");
        const budgets = tickMetrics?.metrics.find((family) => family.name === "ambrose_world_tick_subsystem_budget_nanoseconds");
        const available = tickMetrics?.metrics.find((family) => family.name === "ambrose_world_tick_subsystem_available");
        const overBudget = tickMetrics?.metrics.find((family) => family.name === "ambrose_world_tick_subsystem_over_budget");
        return (elapsed?.series ?? []).map((series) => {
            const name = series.labels.component ?? "unknown";
            const reason = available?.series.find((one) => one.labels.component === name)?.labels.reason ?? "";
            return {
                name,
                title:
                    (
                        {
                            network_drain: "Network queue drain",
                            session_update: "Session world update",
                            session_cleanup: "Session cleanup",
                            zone_instances: "Zone instances",
                            scripting: "Scripting",
                            world_overhead: "Other world tick work",
                            movement: "Movement",
                            chat: "Chat",
                            database_waits: "Database waits",
                            combat: "Combat",
                        } as Record<string, string>
                    )[name] ?? name.replaceAll("_", " "),
                nanoseconds: series.value,
                budget: componentValue(name, budgets),
                available: componentValue(name, available) === 1,
                overBudget: componentValue(name, overBudget) === 1,
                reason,
            };
        });
    });

    async function captureTickProfile() {
        const target = gameApp;
        if (target === "") return;
        captureActive = true;
        captureMessage = "";
        try {
            let profile = await startTickProfile(target, captureSeconds);
            const deadline = Date.now() + (captureSeconds + 5) * 1000;
            while (profile.active && Date.now() < deadline) {
                await new Promise((resolve) => setTimeout(resolve, 250));
                profile = await tickProfileOf(target);
            }
            if (profile.active || !profile.complete) throw new Error("The tick profile did not finish before its deadline");
            const answer = await tickProfileTraceOf(target);
            const url = URL.createObjectURL(new Blob([answer.trace], { type: "application/json" }));
            const link = document.createElement("a");
            link.href = url;
            link.download = "world-tick-profile.json";
            document.body.append(link);
            link.click();
            link.remove();
            setTimeout(() => URL.revokeObjectURL(url), 1000);
            captureMessage = `Downloaded ${answer.events} Chrome trace events${answer.truncated ? "; capture reached its event limit" : ""}.`;
        } catch (problem) {
            captureMessage =
                problem instanceof ApiError
                    ? problem.message
                    : problem instanceof Error
                      ? problem.message
                      : "The tick profile could not be captured";
        } finally {
            captureActive = false;
        }
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
        <MaintenanceCard />
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
            <Card.Action>
                <StatusBadge tone={overallState.tone} pulse={overallState.tone === "healthy" && !stale}>{overallState.word}</StatusBadge
                ></Card.Action
            >
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
            {#if status?.tick && gameApp !== ""}
                <section class="mt-5 border-t pt-4" aria-labelledby="tick-breakdown-title">
                    <div class="mb-3 flex flex-wrap items-center justify-between gap-3">
                        <div>
                            <h3 id="tick-breakdown-title" class="font-serif text-lg">World tick breakdown</h3>
                            <p class="text-xs text-muted-foreground">
                                Latest measured subsystem time and its budget. Movement is wizards being shown to each other, their jumps
                                and the movement flush; reading what a client sent counts as network drain, and database waits run off the
                                world thread.
                            </p>
                        </div>
                        <div class="flex items-center gap-2">
                            <label for="tick-profile-seconds" class="text-xs text-muted-foreground">Profile</label>
                            <select
                                id="tick-profile-seconds"
                                bind:value={captureSeconds}
                                class="h-9 rounded-md border bg-background px-2 text-sm"
                            >
                                <option value={1}>1 second</option>
                                <option value={5}>5 seconds</option>
                                <option value={10}>10 seconds</option>
                                <option value={30}>30 seconds</option>
                            </select>
                            <Button size="sm" variant="outline" disabled={captureActive} onclick={() => void captureTickProfile()}>
                                {captureActive ? "Capturing…" : "Capture trace"}
                            </Button>
                        </div>
                    </div>
                    {#if tickMetricsFailure !== ""}
                        <p class="mb-3 text-sm text-destructive" role="alert">{tickMetricsFailure}</p>
                    {:else if tickComponents.length === 0}
                        <p class="mb-3 text-sm text-muted-foreground">Waiting for the first world tick measurement.</p>
                    {:else}
                        <div class="overflow-x-auto">
                            <table class="w-full text-sm">
                                <thead>
                                    <tr class="border-b text-left text-xs text-muted-foreground">
                                        <th class="py-2 font-medium">Subsystem</th>
                                        <th class="py-2 text-right font-medium">Latest</th>
                                        <th class="py-2 text-right font-medium">Budget</th>
                                        <th class="py-2 text-right font-medium">State</th>
                                    </tr>
                                </thead>
                                <tbody>
                                    {#each tickComponents as component (component.name)}
                                        <tr class="border-b last:border-0">
                                            <th scope="row" class="py-2 text-left font-medium">{component.title}</th>
                                            <td class="py-2 text-right font-mono tabular-nums">
                                                {component.available && component.nanoseconds !== null
                                                    ? `${(component.nanoseconds / 1000000).toFixed(3)} ms`
                                                    : "Unavailable"}
                                            </td>
                                            <td class="py-2 text-right font-mono tabular-nums">
                                                {component.available && component.budget !== null
                                                    ? `${(component.budget / 1000000).toFixed(3)} ms`
                                                    : "—"}
                                            </td>
                                            <td class="py-2 text-right">
                                                {#if !component.available}
                                                    <StatusBadge tone="unknown">
                                                        {component.reason.includes("not integrated")
                                                            ? "Not landed"
                                                            : component.reason.includes("outside the world tick")
                                                              ? "Outside tick"
                                                              : "Not separately measured"}
                                                    </StatusBadge>
                                                {:else if component.overBudget}
                                                    <StatusBadge tone="wrong">Over budget</StatusBadge>
                                                {:else}
                                                    <StatusBadge tone="healthy">Within budget</StatusBadge>
                                                {/if}
                                            </td>
                                        </tr>
                                    {/each}
                                </tbody>
                            </table>
                        </div>
                    {/if}
                    {#if captureMessage !== ""}
                        <p class="mt-3 text-sm" role="status">{captureMessage}</p>
                    {/if}
                </section>
            {/if}
        </Card.Content>
    </Card.Root>
</div>
