<!-- Project Ambrose by Imjustchico: The servers page, live from the supervisor: every app with its state and the step a start is on, process, place, build, uptime and crash count, start, stop, restart and kill each with a countdown where one applies and a confirmation for the ones that end a run, and the output the supervisor captured for this run and the one before, which is what an app with its admin API off still shows, drawn as the four columns doc/DESIGN.md settles so a level, a category and a value each read apart from the words around them. Each power control is there only for an operator the server would let use it, hidden by the same permission it checks, so the page never offers what would come back refused. -->
<script lang="ts">
    import * as Card from "$lib/components/ui/card/index.js";
    import * as Dialog from "$lib/components/ui/dialog/index.js";
    import * as DropdownMenu from "$lib/components/ui/dropdown-menu/index.js";
    import * as Select from "$lib/components/ui/select/index.js";
    import * as Table from "$lib/components/ui/table/index.js";
    import * as Tabs from "$lib/components/ui/tabs/index.js";
    import { Button } from "$lib/components/ui/button/index.js";
    import { Input } from "$lib/components/ui/input/index.js";
    import { Label } from "$lib/components/ui/label/index.js";
    import { ApiError } from "$lib/api.svelte.js";
    import { formatUptime } from "$lib/format.js";
    import { live } from "$lib/status.svelte.js";
    import LogView from "$lib/components/LogView.svelte";
    import { may } from "$lib/permission.svelte.js";
    import { crashes, output, power, supervised, supervisorServes, type OutputRun, type PowerAction } from "$lib/supervision.svelte.js";
    import type { AppEntry, CrashRecord, OutputAnswer } from "$lib/schemas.js";
    import EllipsisIcon from "@lucide/svelte/icons/ellipsis";
    import PlayIcon from "@lucide/svelte/icons/play";
    import PowerIcon from "@lucide/svelte/icons/power";
    import RotateCcwIcon from "@lucide/svelte/icons/rotate-ccw";
    import { toast } from "svelte-sonner";
    import PageHeader from "../components/PageHeader.svelte";
    import StatusBadge from "../components/StatusBadge.svelte";

    type Tone = "healthy" | "waiting" | "wrong" | "unknown";

    const tones: Record<string, Tone> = {
        running: "healthy",
        starting: "waiting",
        stopping: "waiting",
        crashed: "wrong",
        backoff: "waiting",
        crash_loop: "wrong",
        offline: "unknown",
    };

    const classificationWords: Record<string, string> = {
        requested: "Requested",
        clean_unexpected: "Clean but unexpected",
        crash: "Crash",
        out_of_memory: "Out of memory",
        startup_failure: "Startup failure",
    };

    const words: Record<PowerAction, string> = { start: "Start", stop: "Stop", restart: "Restart", kill: "Kill" };

    const apps = $derived(supervised());
    const served = $derived(supervisorServes());
    let chosen = $state("");
    let run = $state<OutputRun>("current");
    let lines = $state<OutputAnswer | null>(null);
    let outputError = $state("");
    let history = $state<CrashRecord[] | null>(null);
    let historyError = $state("");
    let asking = $state(false);
    let askApp = $state("");
    let askAction = $state<PowerAction>("stop");
    let askSeconds = $state("0");
    let working = $state("");

    const app = $derived(apps.find((entry) => entry.name === chosen) ?? apps[0]);

    $effect(() => {
        if (apps.length > 0 && !apps.some((entry) => entry.name === chosen)) chosen = apps[0].name;
    });

    $effect(() => {
        const name = app?.name;
        const which = run;
        if (name === undefined) return;
        const controller = new AbortController();
        let timer: ReturnType<typeof setInterval> | undefined;
        const read = async () => {
            try {
                lines = await output(name, which, controller.signal);
                outputError = "";
            } catch (failure) {
                if (controller.signal.aborted) return;
                outputError = failure instanceof ApiError ? failure.message : "The output could not be read";
            }
        };
        void read();
        timer = setInterval(() => void read(), 2000);
        return () => {
            clearInterval(timer);
            controller.abort();
        };
    });

    function appearance(entry: AppEntry): { tone: Tone; word: string } {
        const supervision = entry.supervision;
        if (!supervision) return { tone: "unknown", word: "Not supervised" };
        if (supervision.crash_loop) return { tone: "wrong", word: "Crash looping" };
        if (supervision.state === "backoff" || (supervision.state === "crashed" && supervision.restart_epoch_ms))
            return { tone: "waiting", word: "Starting again" };
        const word = supervision.state.charAt(0).toUpperCase() + supervision.state.slice(1);
        return { tone: tones[supervision.state] ?? "unknown", word };
    }

    function exitSummary(record: CrashRecord): string {
        const parts = [classificationWords[record.classification] ?? record.classification];
        if (record.signal !== null && record.signal !== undefined) parts.push(`signal ${record.signal}`);
        else if (record.code !== null && record.code !== undefined) parts.push(`code ${record.code}`);
        parts.push(`after ${formatUptime(Math.max(0, Math.round(record.uptime_ms / 1000)))}`);
        return parts.join(" · ");
    }

    function uptime(entry: AppEntry): string {
        const started = entry.supervision?.started_epoch_ms ?? 0;
        const running = entry.supervision?.state === "running" || entry.supervision?.state === "starting";
        if (!running || started === 0) return "Not running";
        return formatUptime(Math.max(0, Math.round((live.now - started) / 1000)));
    }

    $effect(() => {
        const name = app?.name;
        if (name === undefined) return;
        const controller = new AbortController();
        history = null;
        historyError = "";
        void crashes(name, controller.signal)
            .then((records) => {
                history = [...records].reverse();
            })
            .catch((failure) => {
                if (controller.signal.aborted) return;
                historyError = failure instanceof ApiError ? failure.message : "The crash history could not be read";
            });
        return () => controller.abort();
    });

    function needsAsking(action: PowerAction): boolean {
        return action !== "start";
    }

    function begin(name: string, action: PowerAction) {
        if (!needsAsking(action)) {
            void send(name, action, 0);
            return;
        }
        askApp = name;
        askAction = action;
        askSeconds = "0";
        asking = true;
    }

    async function send(name: string, action: PowerAction, seconds: number) {
        working = `${name}:${action}`;
        try {
            await power(name, action, seconds);
            toast(`${words[action]} ${name}`, {
                description: seconds > 0 ? `The supervisor stops it after ${seconds} seconds.` : "The supervisor is carrying it out.",
            });
        } catch (failure) {
            const problem = failure instanceof ApiError ? failure : null;
            toast.error(`${words[action]} ${name} was refused`, {
                description: problem
                    ? `${problem.message}${problem.requestId ? ` (request ${problem.requestId})` : ""}`
                    : "The supervisor did not answer",
            });
        } finally {
            working = "";
        }
    }

    function confirm() {
        const seconds = Number.parseInt(askSeconds, 10);
        asking = false;
        void send(askApp, askAction, Number.isFinite(seconds) && seconds > 0 ? seconds : 0);
    }
</script>

<PageHeader
    title="Servers"
    description={served
        ? "Every app the supervisor runs on this machine, live."
        : "The app that served this panel. The supervisor runs the others."}
/>

{#if !served}
    <Card.Root class="shadow-xs">
        <Card.Header>
            <Card.Title>The supervisor is not serving this panel</Card.Title>
            <Card.Description>
                {live.status?.app ?? "This app"} served the panel itself, so it knows only about itself. Start the supervisor, which runs loginserver,
                gameserver and patchserver, and open the panel from it to start, stop and restart them here.
            </Card.Description>
        </Card.Header>
    </Card.Root>
{:else}
    <Card.Root class="py-0 shadow-xs">
        <Table.Root>
            <Table.Header>
                <Table.Row class="hover:bg-transparent">
                    <Table.Head class="pl-6">App</Table.Head>
                    <Table.Head>State</Table.Head>
                    <Table.Head class="hidden md:table-cell">Address</Table.Head>
                    <Table.Head class="hidden sm:table-cell">Uptime</Table.Head>
                    <Table.Head class="hidden lg:table-cell">Process</Table.Head>
                    <Table.Head class="hidden lg:table-cell">Crashes</Table.Head>
                    <Table.Head class="pr-6 text-right">Actions</Table.Head>
                </Table.Row>
            </Table.Header>
            <Table.Body>
                {#each apps as entry (entry.name)}
                    {@const shown = appearance(entry)}
                    {@const supervision = entry.supervision}
                    {@const alive =
                        supervision?.state === "running" || supervision?.state === "starting" || supervision?.state === "stopping"}
                    <Table.Row class={entry.name === app?.name ? "bg-muted/40" : ""}>
                        <Table.Cell class="pl-6">
                            <button class="text-left" onclick={() => (chosen = entry.name)}>
                                <div class="font-medium">{entry.name}</div>
                                <div class="text-xs text-muted-foreground">
                                    {entry.role}{entry.realm ? ` · ${entry.realm}` : ""}{supervision?.adopted ? " · taken back" : ""}
                                </div>
                            </button>
                        </Table.Cell>
                        <Table.Cell>
                            <StatusBadge tone={shown.tone} pulse={shown.tone === "healthy"}>{shown.word}</StatusBadge>
                            {#if supervision?.state === "starting" && supervision.start}<div
                                    class="mt-1 max-w-64 text-xs text-muted-foreground"
                                >
                                    Now {supervision.start.stage}
                                </div>{/if}
                            {#if supervision?.message}<div class="mt-1 max-w-64 text-xs text-muted-foreground">
                                    {supervision.message}
                                </div>{/if}
                            {#if supervision?.lock}<div class="mt-1 max-w-64 text-xs text-muted-foreground">
                                    Held by {supervision.lock.holder}'s {supervision.lock.action}
                                </div>{/if}
                        </Table.Cell>
                        <Table.Cell class="hidden font-mono text-xs md:table-cell"
                            >{entry.address === "" || entry.port === 0 ? "—" : `${entry.address}:${entry.port}`}</Table.Cell
                        >
                        <Table.Cell class="hidden sm:table-cell">{uptime(entry)}</Table.Cell>
                        <Table.Cell class="hidden font-mono text-xs tabular-nums lg:table-cell">{supervision?.pid ?? "—"}</Table.Cell>
                        <Table.Cell class="hidden tabular-nums lg:table-cell">{supervision?.crashes ?? 0}</Table.Cell>
                        <Table.Cell class="pr-6">
                            <div class="flex items-center justify-end gap-2">
                                {#if alive}
                                    {#if may("power.restart", entry.name)}
                                        <Button
                                            size="sm"
                                            variant="outline"
                                            disabled={working !== ""}
                                            onclick={() => begin(entry.name, "restart")}><RotateCcwIcon />Restart</Button
                                        >
                                    {/if}
                                    {#if may("power.stop", entry.name)}
                                        <Button
                                            size="sm"
                                            variant="outline"
                                            class="text-destructive hover:text-destructive"
                                            disabled={working !== ""}
                                            onclick={() => begin(entry.name, "stop")}><PowerIcon />Stop</Button
                                        >
                                    {/if}
                                {:else if may("power.start", entry.name)}
                                    <Button size="sm" disabled={working !== ""} onclick={() => begin(entry.name, "start")}
                                        ><PlayIcon />Start</Button
                                    >
                                {/if}
                                <DropdownMenu.Root>
                                    <DropdownMenu.Trigger>
                                        {#snippet child({ props })}
                                            <Button
                                                variant="ghost"
                                                size="icon"
                                                class="size-8"
                                                aria-label={`More for ${entry.name}`}
                                                {...props}><EllipsisIcon /></Button
                                            >
                                        {/snippet}
                                    </DropdownMenu.Trigger>
                                    <DropdownMenu.Content align="end">
                                        <DropdownMenu.Item onclick={() => (chosen = entry.name)}>Show its output</DropdownMenu.Item>
                                        <DropdownMenu.Item disabled={!alive} onclick={() => begin(entry.name, "kill")}
                                            >Kill its process tree</DropdownMenu.Item
                                        >
                                    </DropdownMenu.Content>
                                </DropdownMenu.Root>
                            </div>
                        </Table.Cell>
                    </Table.Row>
                {/each}
                {#if apps.length === 0}
                    <Table.Row>
                        <Table.Cell colspan={7} class="py-8 text-center text-sm text-muted-foreground"
                            >The supervisor runs no app. Name them in Supervisor.Apps.</Table.Cell
                        >
                    </Table.Row>
                {/if}
            </Table.Body>
        </Table.Root>
    </Card.Root>

    {#if app}
        <Card.Root class="shadow-xs">
            <Card.Header>
                <Card.Title>Output of {app.name}</Card.Title>
                <Card.Description>
                    What the supervisor captured from this app, read again every two seconds. It holds what the app wrote before its admin
                    API was up, after it exited, and while its admin API is off.
                </Card.Description>
                <Card.Action>
                    <div class="flex items-center gap-2">
                        {#if apps.length > 1}
                            <Select.Root type="single" bind:value={chosen}>
                                <Select.Trigger class="w-40" aria-label="App whose output is shown">{app.name}</Select.Trigger>
                                <Select.Content>
                                    {#each apps as entry (entry.name)}
                                        <Select.Item value={entry.name}>{entry.name}</Select.Item>
                                    {/each}
                                </Select.Content>
                            </Select.Root>
                        {/if}
                        <Tabs.Root value={run} onValueChange={(value) => (run = value as OutputRun)}>
                            <Tabs.List>
                                <Tabs.Trigger value="current">This run</Tabs.Trigger>
                                <Tabs.Trigger value="previous">Previous run</Tabs.Trigger>
                            </Tabs.List>
                        </Tabs.Root>
                    </div>
                </Card.Action>
            </Card.Header>
            <Card.Content>
                {#if outputError}
                    <p class="text-sm text-destructive">{outputError}</p>
                {:else if !lines || lines.lines.length === 0}
                    <p class="text-sm text-muted-foreground">
                        {run === "current" ? "Nothing captured for this run yet." : "There is no previous run."}
                    </p>
                {:else}
                    <LogView lines={lines.lines} label={`Output of ${app.name}`} />
                {/if}
            </Card.Content>
        </Card.Root>

        <Card.Root class="shadow-xs">
            <Card.Header>
                <Card.Title>Crash history of {app.name}</Card.Title>
                <Card.Description>
                    Every crash the supervisor recorded, newest first, each with its classification, exit code or signal, uptime and the
                    last 200 log lines. Dumps are written by a later milestone; until then each record says so.
                </Card.Description>
            </Card.Header>
            <Card.Content>
                {#if historyError}
                    <p class="text-sm text-destructive">{historyError}</p>
                {:else if history === null}
                    <p class="text-sm text-muted-foreground">Reading the crash history…</p>
                {:else if history.length === 0}
                    <p class="text-sm text-muted-foreground">No crashes recorded for {app.name}.</p>
                {:else}
                    <div class="space-y-4">
                        {#each history as record (record.epoch_ms)}
                            <div class="rounded-md border p-4">
                                <div class="flex flex-wrap items-center gap-2">
                                    <StatusBadge
                                        tone={record.classification === "crash" || record.classification === "out_of_memory"
                                            ? "wrong"
                                            : "waiting"}
                                    >
                                        {classificationWords[record.classification] ?? record.classification}
                                    </StatusBadge>
                                    <span class="text-sm text-muted-foreground">{new Date(record.epoch_ms).toLocaleString()}</span>
                                    <span class="text-sm text-muted-foreground">{exitSummary(record)}</span>
                                </div>
                                <p class="mt-2 text-xs text-muted-foreground">
                                    {record.dump_written ? "A dump was written." : record.dump_note}
                                </p>
                                {#if record.log_tail.length > 0}
                                    <details class="mt-2">
                                        <summary class="cursor-pointer text-sm text-muted-foreground"
                                            >Last {record.log_tail.length} log lines</summary
                                        >
                                        <pre
                                            class="mt-2 max-h-64 overflow-auto rounded bg-muted/40 p-3 font-mono text-xs whitespace-pre-wrap">{record.log_tail.join(
                                                "\n",
                                            )}</pre>
                                    </details>
                                {/if}
                            </div>
                        {/each}
                    </div>
                {/if}
            </Card.Content>
        </Card.Root>
    {/if}
{/if}

<Dialog.Root bind:open={asking}>
    <Dialog.Content>
        <Dialog.Header>
            <Dialog.Title>{words[askAction]} {askApp}?</Dialog.Title>
            <Dialog.Description>
                {#if askAction === "kill"}
                    This ends the app's whole process tree at once. It saves nothing and tells nobody.
                {:else}
                    The supervisor asks the app to shut down through its admin API, then on its input, then with an interrupt, and ends its
                    process tree if it has not stopped when the stop timeout passes.
                {/if}
            </Dialog.Description>
        </Dialog.Header>
        {#if askAction !== "kill"}
            <div class="space-y-2">
                <Label for="countdown">Countdown in seconds</Label>
                <Input id="countdown" type="number" min="0" max="86400" bind:value={askSeconds} />
                <p class="text-xs text-muted-foreground">
                    The app counts down itself, so a login server tells the players it is stopping. Zero stops it now.
                </p>
            </div>
        {/if}
        <Dialog.Footer>
            <Button variant="outline" onclick={() => (asking = false)}>Leave it</Button>
            <Button variant={askAction === "kill" ? "destructive" : "default"} onclick={confirm}>{words[askAction]} it</Button>
        </Dialog.Footer>
    </Dialog.Content>
</Dialog.Root>
