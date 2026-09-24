<!-- Project Ambrose by Imjustchico: The log viewer, live: a tab per app the supervisor runs, reading each app's own log after the last sequence number it saw and appending what came back, so a page that was away sees what it missed rather than starting again. Level and category filters with their counts, a search that marks what it finds, following with a count of what arrived while paused, jumping back to the newest, wrapping on request, and opening a record to read its fields. Lines the app wrote faster than this page could read them are said to be gone rather than quietly skipped, and a read that fails says so instead of the page looking merely quiet. -->
<script lang="ts">
    import * as Card from "$lib/components/ui/card/index.js";
    import * as DropdownMenu from "$lib/components/ui/dropdown-menu/index.js";
    import * as Tabs from "$lib/components/ui/tabs/index.js";
    import * as ToggleGroup from "$lib/components/ui/toggle-group/index.js";
    import { Button } from "$lib/components/ui/button/index.js";
    import { Input } from "$lib/components/ui/input/index.js";
    import { Label } from "$lib/components/ui/label/index.js";
    import { Switch } from "$lib/components/ui/switch/index.js";
    import ArrowDownIcon from "@lucide/svelte/icons/arrow-down";
    import CopyIcon from "@lucide/svelte/icons/copy";
    import ListFilterIcon from "@lucide/svelte/icons/list-filter";
    import PauseIcon from "@lucide/svelte/icons/pause";
    import PlayIcon from "@lucide/svelte/icons/play";
    import SearchIcon from "@lucide/svelte/icons/search";
    import { tick } from "svelte";
    import { toast } from "svelte-sonner";
    import PageHeader from "../components/PageHeader.svelte";
    import StatusBadge from "../components/StatusBadge.svelte";
    import { focus } from "../focus.svelte";
    import { supervised, logsAfter } from "$lib/supervision.svelte.js";
    import type { LogRecord } from "$lib/schemas.js";

    const order = ["trace", "debug", "info", "warn", "error", "fatal"] as const;
    const chips: Record<string, string> = {
        trace: "text-faint",
        debug: "text-faint",
        info: "bg-healthy/10 text-healthy",
        warn: "bg-waiting/10 text-waiting",
        error: "bg-destructive/10 text-destructive",
        fatal: "bg-destructive text-background",
    };
    const dots: Record<string, string> = {
        trace: "bg-faint",
        debug: "bg-faint",
        info: "bg-healthy",
        warn: "bg-waiting",
        error: "bg-destructive",
        fatal: "bg-destructive",
    };

    const apps = $derived(supervised());
    let records = $state.raw<LogRecord[]>([]);
    let after = 0;
    let reading = false;
    let trouble = $state("");
    let levels = $state<string[]>(["debug", "info", "warn", "error", "fatal"]);
    let hidden = $state<string[]>([]);
    let search = $state("");
    let live = $state(true);
    let wrap = $state(false);
    let opened = $state<number | null>(null);
    let unseen = $state(0);
    let atBottom = true;
    let viewport = $state<HTMLDivElement | null>(null);

    const needle = $derived(search.trim().toLowerCase());
    const passes = (record: LogRecord) =>
        levels.includes(record.level) &&
        !hidden.includes(record.category) &&
        (needle === "" || `${record.category} ${record.message}`.toLowerCase().includes(needle));
    const shown = $derived(records.filter(passes));
    const counts = $derived(Object.fromEntries(order.map((level) => [level, records.filter((record) => record.level === level).length])));
    const categories = $derived([...new Set(records.map((record) => record.category))].sort());

    function bracketed(category: string) {
        return `[${category.length > 18 ? `${category.slice(0, 8)}…${category.slice(-9)}` : category}]`;
    }

    function pieces(message: string) {
        if (needle === "") return [{ text: message, hit: false }];
        const parts: { text: string; hit: boolean }[] = [];
        const lower = message.toLowerCase();
        let from = 0;
        for (let at = lower.indexOf(needle); at !== -1; at = lower.indexOf(needle, from)) {
            if (at > from) parts.push({ text: message.slice(from, at), hit: false });
            parts.push({ text: message.slice(at, at + needle.length), hit: true });
            from = at + needle.length;
        }
        if (from < message.length) parts.push({ text: message.slice(from), hit: false });
        return parts;
    }

    function showCategory(category: string, on: boolean) {
        hidden = on ? hidden.filter((entry) => entry !== category) : [...hidden, category];
    }

    function clearFilters() {
        levels = [...order];
        hidden = [];
        search = "";
    }

    function pinToNewest() {
        requestAnimationFrame(() => {
            if (viewport) viewport.scrollTop = viewport.scrollHeight;
        });
    }

    function toNewest() {
        atBottom = true;
        unseen = 0;
        pinToNewest();
    }

    function followScroll() {
        if (!viewport) return;
        atBottom = viewport.scrollHeight - viewport.scrollTop - viewport.clientHeight < 24;
        if (atBottom) unseen = 0;
    }

    async function toggle(sequence: number) {
        opened = opened === sequence ? null : sequence;
        await tick();
        if (opened !== null) document.getElementById(`record-${opened}`)?.scrollIntoView({ block: "nearest" });
    }

    async function copy(record: LogRecord) {
        try {
            await navigator.clipboard.writeText(JSON.stringify({ app: focus.app, ...record }));
            toast.success(`Copied record ${record.sequence}`);
        } catch {
            toast.error("The browser did not allow copying");
        }
    }

    $effect(() => {
        void shown;
        if (viewport && atBottom) pinToNewest();
    });

    $effect(() => {
        const name = focus.app;
        records = [];
        after = 0;
        unseen = 0;
        trouble = "";
        return () => {
            void name;
        };
    });

    $effect(() => {
        if (!live) return;
        const name = focus.app;
        let stopped = false;
        const controller = new AbortController();

        async function read() {
            if (reading || stopped || name === "") return;
            reading = true;
            try {
                const answer = await logsAfter(name, after, controller.signal);
                if (stopped) return;
                trouble = "";
                if (answer.dropped)
                    records = [
                        ...records,
                        {
                            sequence: answer.dropped.from,
                            time: "",
                            epoch_ms: 0,
                            level: "warn",
                            category: "logs",
                            message: `${answer.dropped.count} line(s) were written faster than this page could read them and are gone`,
                        } as LogRecord,
                    ];
                if (answer.records.length > 0) {
                    after = answer.records[answer.records.length - 1].sequence;
                    records = [...records, ...answer.records].slice(-2000);
                    if (!atBottom) unseen += answer.records.filter(passes).length;
                }
            } catch (failure) {
                if (!stopped) trouble = failure instanceof Error ? failure.message : String(failure);
            } finally {
                reading = false;
            }
        }

        void read();
        const timer = setInterval(() => void read(), 1000);
        return () => {
            stopped = true;
            controller.abort();
            clearInterval(timer);
        };
    });
</script>

<PageHeader title="Logs" description="Follow a server's log as it is written.">
    {#snippet actions()}
        {#if live}
            <StatusBadge tone="healthy" pulse>Following live</StatusBadge>
            <Button variant="outline" size="sm" onclick={() => (live = false)}><PauseIcon />Pause</Button>
        {:else}
            <StatusBadge tone="waiting">Paused</StatusBadge>
            <Button variant="outline" size="sm" onclick={() => (live = true)}><PlayIcon />Resume</Button>
        {/if}
    {/snippet}
</PageHeader>

{#snippet viewer()}
    <Card.Root class="gap-0 overflow-hidden py-0 shadow-xs">
        {#if trouble !== ""}
            <div class="border-b bg-destructive/10 px-4 py-2 text-sm text-destructive" role="status">
                The log could not be read: {trouble}
            </div>
        {/if}
        <div class="relative">
            <div
                bind:this={viewport}
                onscroll={followScroll}
                class="h-[60vh] overflow-y-auto bg-sidebar/60 py-2 font-mono text-xs leading-6"
                role="log"
                aria-label={`Log records from ${focus.app}`}
            >
                {#each shown as record, index (record.sequence)}
                    {@const quiet = record.level === "trace" || record.level === "debug"}
                    {@const fresh = index === 0 || shown[index - 1].time.slice(11, 19) !== record.time.slice(11, 19)}
                    <div class={opened === record.sequence ? "bg-muted/60" : ""}>
                        <button
                            type="button"
                            class={`grid w-full grid-cols-[12ch_6ch_minmax(0,1fr)] gap-x-2 px-4 text-left hover:bg-muted/60 focus-visible:bg-muted/60 focus-visible:outline-none md:grid-cols-[12ch_6ch_20ch_minmax(0,1fr)] ${quiet ? "text-muted-foreground" : "text-foreground"}`}
                            aria-expanded={opened === record.sequence}
                            onclick={() => toggle(record.sequence)}
                        >
                            <span class={fresh ? "text-muted-foreground" : "text-faint"}>{record.time.slice(11)}</span>
                            <span class={`self-start rounded-sm text-center uppercase ${chips[record.level]}`}>{record.level}</span>
                            <span class="hidden truncate text-muted-foreground md:inline">{bracketed(record.category)}</span>
                            <span class={wrap ? "break-words whitespace-pre-wrap" : "truncate"}
                                >{#each pieces(record.message) as piece, part (part)}{#if piece.hit}<mark
                                            class="rounded-sm bg-foreground/15 text-inherit ring-1 ring-foreground/25">{piece.text}</mark
                                        >{:else}{piece.text}{/if}{/each}</span
                            >
                        </button>
                        {#if opened === record.sequence}
                            <div id={`record-${record.sequence}`} class="mx-4 mb-2 rounded-md border bg-card p-3 font-sans md:ml-40">
                                <dl class="grid grid-cols-[auto_1fr] gap-x-6 gap-y-1 text-xs">
                                    <dt class="text-muted-foreground">Sequence</dt>
                                    <dd class="font-mono">{record.sequence}</dd>
                                    <dt class="text-muted-foreground">Time</dt>
                                    <dd class="font-mono">{record.time}</dd>
                                    <dt class="text-muted-foreground">App</dt>
                                    <dd class="font-mono">{focus.app}</dd>
                                    <dt class="text-muted-foreground">Level</dt>
                                    <dd class="font-mono">{record.level}</dd>
                                    <dt class="text-muted-foreground">Category</dt>
                                    <dd class="font-mono">{record.category}</dd>
                                    <dt class="text-muted-foreground">Message</dt>
                                    <dd class="font-mono break-words">{record.message}</dd>
                                </dl>
                                <Button size="sm" variant="outline" class="mt-3" onclick={() => copy(record)}
                                    ><CopyIcon />Copy as JSON</Button
                                >
                            </div>
                        {/if}
                    </div>
                {:else}
                    <div class="flex flex-col items-center gap-3 py-12 font-sans text-sm text-muted-foreground">
                        <p>No record matches these levels, categories and search.</p>
                        <Button size="sm" variant="outline" onclick={clearFilters}>Clear the filters</Button>
                    </div>
                {/each}
            </div>
            {#if unseen > 0}
                <Button size="sm" class="absolute bottom-3 left-1/2 -translate-x-1/2 shadow-md" onclick={toNewest}>
                    <ArrowDownIcon />{unseen} new record{unseen === 1 ? "" : "s"}
                </Button>
            {/if}
        </div>
        <div class="flex items-center justify-between border-t px-4 py-2 text-xs text-muted-foreground">
            <span>Showing {shown.length} of {records.length} records, newest at the bottom</span>
            <span class="hidden sm:inline">Sample data for {focus.app}</span>
        </div>
    </Card.Root>
{/snippet}

<Tabs.Root bind:value={focus.app} class="gap-4">
    <Tabs.List aria-label="App whose log to follow">
        {#each apps as entry (entry.name)}<Tabs.Trigger value={entry.name} class="font-mono text-xs">{entry.name}</Tabs.Trigger>{/each}
    </Tabs.List>
    <div class="flex flex-col gap-3 @4xl/main:flex-row @4xl/main:items-center">
        <div class="relative flex-1">
            <SearchIcon class="absolute top-1/2 left-2.5 size-4 -translate-y-1/2 text-muted-foreground" />
            <Input type="search" placeholder="Search logs" class="pl-8" aria-label="Search messages and categories" bind:value={search} />
        </div>
        <div class="flex flex-wrap items-center gap-3">
            <ToggleGroup.Root
                type="multiple"
                variant="outline"
                size="sm"
                spacing={1}
                class="flex-wrap"
                bind:value={levels}
                aria-label="Levels to show"
            >
                {#each order as level (level)}
                    <ToggleGroup.Item value={level} class="gap-1.5 px-2.5">
                        <span class={`size-1.5 rounded-full ${dots[level]}`}></span>{level}
                        <span class="font-mono text-xs tabular-nums opacity-70">{counts[level]}</span>
                    </ToggleGroup.Item>
                {/each}
            </ToggleGroup.Root>
            <DropdownMenu.Root>
                <DropdownMenu.Trigger>
                    {#snippet child({ props })}
                        <Button variant="outline" size="sm" {...props}>
                            <ListFilterIcon />Categories
                            {#if hidden.length > 0}
                                <span class="font-mono text-xs tabular-nums opacity-70"
                                    >{categories.length - hidden.length}/{categories.length}</span
                                >
                            {/if}
                        </Button>
                    {/snippet}
                </DropdownMenu.Trigger>
                <DropdownMenu.Content align="end" class="w-60">
                    <DropdownMenu.Label>Categories to show</DropdownMenu.Label>
                    <DropdownMenu.Separator />
                    {#each categories as category (category)}
                        <DropdownMenu.CheckboxItem
                            checked={!hidden.includes(category)}
                            onCheckedChange={(on) => showCategory(category, on)}
                            closeOnSelect={false}
                            class="font-mono text-xs">{category}</DropdownMenu.CheckboxItem
                        >
                    {/each}
                    <DropdownMenu.Separator />
                    <DropdownMenu.Item onSelect={() => (hidden = [])}>Show every category</DropdownMenu.Item>
                </DropdownMenu.Content>
            </DropdownMenu.Root>
            <div class="flex items-center gap-2">
                <Switch id="wrap-lines" bind:checked={wrap} />
                <Label for="wrap-lines" class="text-sm font-normal whitespace-nowrap">Wrap lines</Label>
            </div>
        </div>
    </div>
    {#each apps as entry (entry.name)}
        <Tabs.Content value={entry.name}
            >{#if focus.app === entry.name}{@render viewer()}{/if}</Tabs.Content
        >
    {/each}
</Tabs.Root>
