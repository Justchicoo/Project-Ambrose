<!-- Project Ambrose by Imjustchico: The operations calendar 17.96 draws: one view over the four sources of planned events, schedules, timed game events, installation maintenance and realm maintenance, each in its meaning color, in a week or a month in the operator's own time zone naming the node's zone where it differs, the conflict reasons where a player-facing entry runs across a disruptive one, each entry opening the page that owns it and nothing edited here, the sources whose milestones have not landed named as unavailable, and a refresh every half minute so a changed schedule moves its entry without reopening the page. -->
<script lang="ts">
    import { ApiError } from "$lib/api.svelte.js";
    import { live } from "$lib/status.svelte.js";
    import PageHeader from "../components/PageHeader.svelte";
    import {
        addDaysMs,
        conflictReasonFor,
        formatDay,
        formatRange,
        getCalendar,
        operatorTimeZone,
        sourceLabel,
        startOfMonthMs,
        startOfWeekMs,
        type OpsCalendarAnswer,
        type OpsCalendarEvent,
    } from "$lib/opsCalendar.svelte.js";

    type View = "week" | "month";

    let view = $state<View>("week");
    let anchor = $state(Date.now());
    let answer = $state<OpsCalendarAnswer | null>(null);
    let failure = $state("");

    const zone = operatorTimeZone();

    const range = $derived.by(() => {
        if (view === "week") {
            const from = startOfWeekMs(anchor, zone);
            return { from, to: addDaysMs(from, 7) };
        }
        const from = startOfWeekMs(startOfMonthMs(anchor, zone), zone);
        return { from, to: addDaysMs(from, 42) };
    });

    const days = $derived.by(() => {
        const count = view === "week" ? 7 : 42;
        return Array.from({ length: count }, (_, i) => addDaysMs(range.from, i));
    });

    function eventsOn(dayMs: number): OpsCalendarEvent[] {
        if (!answer) return [];
        const next = addDaysMs(dayMs, 1);
        return answer.events
            .filter((e) => e.start_epoch_ms < next && (e.end_epoch_ms === 0 || e.end_epoch_ms > dayMs))
            .sort((a, b) => a.start_epoch_ms - b.start_epoch_ms);
    }

    const operatorOffset = -new Date().getTimezoneOffset();
    const serverDiffers = $derived(answer !== null && answer.server_utc_offset_minutes !== operatorOffset);
    const serverLabel = $derived.by(() => {
        if (!answer) return "";
        const minutes = answer.server_utc_offset_minutes;
        const sign = minutes < 0 ? "-" : "+";
        const abs = Math.abs(minutes);
        return `UTC${sign}${String(Math.floor(abs / 60)).padStart(2, "0")}:${String(abs % 60).padStart(2, "0")}`;
    });

    $effect(() => {
        const beat = live.now;
        void beat;
        const from = range.from;
        const to = range.to;
        const controller = new AbortController();
        const timer = window.setInterval(() => {
            void refresh(controller.signal, from, to);
        }, 30_000);
        void refresh(controller.signal, from, to);
        return () => {
            window.clearInterval(timer);
            controller.abort();
        };
    });

    async function refresh(signal: AbortSignal, from: number, to: number) {
        try {
            answer = await getCalendar(from, to, signal);
            failure = "";
        } catch (problem) {
            if (signal.aborted) return;
            answer = null;
            failure = problem instanceof ApiError ? problem.message : "The calendar could not be read";
        }
    }

    function shift(daysCount: number) {
        anchor = addDaysMs(anchor, daysCount);
    }
</script>

<PageHeader
    title="Operations calendar"
    description="Everything planned in one view: schedules, timed game events, installation and realm maintenance."
/>

<div class="mb-4 flex flex-wrap items-center gap-2">
    <button
        class="rounded-md border px-3 py-1.5 text-sm {view === 'week' ? 'bg-primary text-primary-foreground' : ''}"
        onclick={() => (view = "week")}>Week</button
    >
    <button
        class="rounded-md border px-3 py-1.5 text-sm {view === 'month' ? 'bg-primary text-primary-foreground' : ''}"
        onclick={() => (view = "month")}>Month</button
    >
    <span class="mx-2 text-muted-foreground">|</span>
    <button class="rounded-md border px-3 py-1.5 text-sm" onclick={() => shift(view === "week" ? -7 : -28)}>‹ Prev</button>
    <button class="rounded-md border px-3 py-1.5 text-sm" onclick={() => (anchor = Date.now())}>Today</button>
    <button class="rounded-md border px-3 py-1.5 text-sm" onclick={() => shift(view === "week" ? 7 : 28)}>Next ›</button>
    <span class="ml-auto text-sm text-muted-foreground">
        All times in {zone}{#if serverDiffers} · node runs at {serverLabel}{/if}
    </span>
</div>

{#if failure}
    <p class="rounded-md border border-destructive p-4 text-sm text-destructive">{failure}</p>
{:else if answer}
    {#if answer.conflicts.length > 0}
        <div class="mb-4 space-y-1 rounded-md border border-amber-500/50 bg-amber-500/10 p-3" role="alert">
            {#each answer.conflicts as conflict (conflict.event_a_id + conflict.event_b_id)}
                <p class="text-sm">⚠ {conflict.reason}</p>
            {/each}
        </div>
    {/if}

    <div class="grid grid-cols-7 gap-px overflow-hidden rounded-md border bg-border" role="grid" aria-label="Operations calendar">
        {#each days as dayMs (dayMs)}
            {@const dayEvents = eventsOn(dayMs)}
            <div class="min-h-24 bg-background p-1.5" role="gridcell" aria-label={formatDay(dayMs)}>
                <p class="mb-1 text-xs font-medium text-muted-foreground">{formatDay(dayMs)}</p>
                <div class="space-y-1">
                    {#each dayEvents as event (event.id)}
                        {@const conflict = conflictReasonFor(event.id, answer.conflicts)}
                        <a
                            href={event.url || undefined}
                            class="block truncate rounded border-l-4 px-1.5 py-1 text-xs hover:bg-accent"
                            style="border-color: {event.color}"
                            title={`${event.title} · ${formatRange(event)}${conflict ? ` · ⚠ ${conflict}` : ""}`}
                        >
                            <span class="font-medium">{event.title}</span>
                            <span class="block text-muted-foreground">{formatRange(event)}</span>
                            {#if conflict}<span class="text-amber-600">⚠ conflict</span>{/if}
                        </a>
                    {/each}
                </div>
            </div>
        {/each}
    </div>

    <div class="mt-4 flex flex-wrap gap-4">
        {#each answer.sources as source (source.source)}
            <span class="flex items-center gap-1.5 text-xs text-muted-foreground">
                <span class="inline-block h-3 w-3 rounded-sm" style="background-color: {source.color}"></span>
                {sourceLabel(source.source)}
                {#if !source.available}<em>· {source.unavailable_reason}</em>{/if}
            </span>
        {/each}
    </div>
{:else}
    <p class="text-sm text-muted-foreground" aria-busy="true">Loading the calendar…</p>
{/if}
