<!-- Project Ambrose by Imjustchico: The servers page, live from the supervisor: every app with its state and the step a start is on or the step of the power operation running on it, process, place, build, uptime and crash count, start, stop, restart and kill each with a countdown where one applies, a reason, a confirmation for the ones that end a run and, for an owner, an override of the protected hours, a restart of the whole stack, disabling an app with a reason and enabling it again, an app held in a protected state showing that state, who holds it and its progress in place of its controls, and the output the supervisor captured for this run and the one before, which is what an app with its admin API off still shows, drawn as the four columns doc/DESIGN.md settles so a level, a category and a value each read apart from the words around them. Each power control is there only for an operator the server would let use it, hidden by the same permission it checks, so the page never offers what would come back refused. -->
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
    import { Switch } from "$lib/components/ui/switch/index.js";
    import { ApiError } from "$lib/api.svelte.js";
    import { formatUptime } from "$lib/format.js";
    import { live } from "$lib/status.svelte.js";
    import LogView from "$lib/components/LogView.svelte";
    import { isOwner, may } from "$lib/permission.svelte.js";
    import { latestStep, runningFor } from "$lib/events.svelte.js";
    import {
        disableApp,
        enableApp,
        heldBy,
        output,
        power,
        powerTarget,
        stateTone,
        stateWord,
        supervised,
        supervisorServes,
        type OutputRun,
        type PowerAction,
    } from "$lib/supervision.svelte.js";
    import type { AppEntry, OutputAnswer } from "$lib/schemas.js";
    import EllipsisIcon from "@lucide/svelte/icons/ellipsis";
    import PlayIcon from "@lucide/svelte/icons/play";
    import PowerIcon from "@lucide/svelte/icons/power";
    import RotateCcwIcon from "@lucide/svelte/icons/rotate-ccw";
    import { toast } from "svelte-sonner";
    import PageHeader from "../components/PageHeader.svelte";
    import StatusBadge from "../components/StatusBadge.svelte";

    type Tone = "healthy" | "waiting" | "wrong" | "unknown";

    const words: Record<PowerAction, string> = { start: "Start", stop: "Stop", restart: "Restart", kill: "Kill" };

    const apps = $derived(supervised());
    const served = $derived(supervisorServes());
    let chosen = $state("");
    let run = $state<OutputRun>("current");
    let lines = $state<OutputAnswer | null>(null);
    let outputError = $state("");
    let asking = $state(false);
    let askApp = $state("");
    let askAction = $state<PowerAction>("stop");
    let askSeconds = $state("0");
    let askReason = $state("");
    let askOverride = $state(false);
    let askStack = $state(false);
    let working = $state("");
    let disabling = $state(false);
    let disableApp_ = $state("");
    let disableReason = $state("");
    const owner = $derived(isOwner());

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
        if (supervision.state === "crashed" && supervision.restart_epoch_ms) return { tone: "waiting", word: "Starting again" };
        return { tone: stateTone(supervision.state), word: stateWord(supervision.state) };
    }

    function uptime(entry: AppEntry): string {
        const started = entry.supervision?.started_epoch_ms ?? 0;
        const running = entry.supervision?.state === "running" || entry.supervision?.state === "starting";
        if (!running || started === 0) return "Not running";
        return formatUptime(Math.max(0, Math.round((live.now - started) / 1000)));
    }

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
        askReason = "";
        askOverride = false;
        askStack = false;
        asking = true;
    }

    function beginStack() {
        askApp = "the stack";
        askAction = "restart";
        askSeconds = "0";
        askReason = "";
        askOverride = false;
        askStack = true;
        asking = true;
    }

    async function send(name: string, action: PowerAction, seconds: number, reason = "", override = false, stack = false) {
        working = `${name}:${action}`;
        const extra = { ...(reason !== "" ? { reason } : {}), ...(override ? { override } : {}) };
        try {
            if (stack) await powerTarget({ kind: "stack", name: null }, action, seconds, extra);
            else await power(name, action, seconds, extra);
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
        void send(askApp, askAction, Number.isFinite(seconds) && seconds > 0 ? seconds : 0, askReason.trim(), askOverride, askStack);
    }

    function beginDisable(name: string) {
        disableApp_ = name;
        disableReason = "";
        disabling = true;
    }

    async function toggle(name: string, disable: boolean, reason = "") {
        working = `${name}:${disable ? "disable" : "enable"}`;
        try {
            if (disable) await disableApp(name, reason);
            else await enableApp(name);
            toast(disable ? `${name} is disabled` : `${name} is enabled`, {
                description: disable ? "It stops, and nothing starts it until it is enabled." : "It starts the next time it is asked to.",
            });
        } catch (failure) {
            const problem = failure instanceof ApiError ? failure : null;
            toast.error(`${disable ? "Disabling" : "Enabling"} ${name} was refused`, {
                description: problem ? problem.message : "The supervisor did not answer",
            });
        } finally {
            working = "";
        }
    }

    function confirmDisable() {
        disabling = false;
        void toggle(disableApp_, true, disableReason.trim());
    }
</script>

<PageHeader
    title="Servers"
    description={served
        ? "Every app the supervisor runs on this machine, live."
        : "The app that served this panel. The supervisor runs the others."}
>
    {#snippet actions()}
        {#if served && apps.length > 1 && apps.every((entry) => may("power.restart", entry.name))}
            <Button variant="outline" disabled={working !== ""} onclick={beginStack}><RotateCcwIcon />Restart the stack</Button>
        {/if}
    {/snippet}
</PageHeader>

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
                    {@const held = heldBy(supervision)}
                    {@const operation = runningFor(entry.name)}
                    {@const step = latestStep(operation, entry.name)}
                    {@const alive =
                        supervision?.process_state !== undefined
                            ? supervision.process_state === "running" ||
                              supervision.process_state === "starting" ||
                              supervision.process_state === "stopping"
                            : supervision?.state === "running" || supervision?.state === "starting" || supervision?.state === "stopping"}
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
                            {#if held}<div class="mt-1 max-w-64 text-xs text-muted-foreground">
                                    For {held.holder}{held.progress ? `: ${held.progress}` : ""}
                                </div>{:else if supervision?.disabled}<div class="mt-1 max-w-64 text-xs text-muted-foreground">
                                    Disabled by {supervision.disabled.by}: {supervision.disabled.reason}
                                </div>{:else if operation}<div class="mt-1 max-w-64 text-xs text-muted-foreground">
                                    {words[operation.action as PowerAction] ?? operation.action} in progress{step
                                        ? `: ${step.message}`
                                        : ""}
                                </div>{:else if supervision?.message}<div class="mt-1 max-w-64 text-xs text-muted-foreground">
                                    {supervision.message}
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
                                {#if held}
                                    <span class="text-xs text-muted-foreground">{stateWord(held.state)}, so power waits</span>
                                {:else if supervision?.disabled}
                                    {#if may("power.disable", entry.name)}
                                        <Button
                                            size="sm"
                                            variant="outline"
                                            disabled={working !== ""}
                                            onclick={() => void toggle(entry.name, false)}><PlayIcon />Enable</Button
                                        >
                                    {/if}
                                {:else if alive}
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
                                        {#if !held}
                                            <DropdownMenu.Item disabled={!alive} onclick={() => begin(entry.name, "kill")}
                                                >Kill its process tree</DropdownMenu.Item
                                            >
                                        {/if}
                                        {#if !held && !supervision?.disabled && may("power.disable", entry.name)}
                                            <DropdownMenu.Item onclick={() => beginDisable(entry.name)}>Disable it</DropdownMenu.Item>
                                        {/if}
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
        <div class="space-y-2">
            <Label for="power-reason">Reason</Label>
            <Input id="power-reason" maxlength={500} placeholder="Kept with the operation in the activity log" bind:value={askReason} />
        </div>
        {#if askAction === "restart" && owner}
            <div class="flex items-center gap-3">
                <Switch id="power-override" bind:checked={askOverride} />
                <Label for="power-override" class="font-normal">Override the protected hours, which needs a reason</Label>
            </div>
        {/if}
        <Dialog.Footer>
            <Button variant="outline" onclick={() => (asking = false)}>Leave it</Button>
            <Button
                variant={askAction === "kill" ? "destructive" : "default"}
                disabled={askOverride && askReason.trim() === ""}
                onclick={confirm}>{words[askAction]} it</Button
            >
        </Dialog.Footer>
    </Dialog.Content>
</Dialog.Root>

<Dialog.Root bind:open={disabling}>
    <Dialog.Content>
        <Dialog.Header>
            <Dialog.Title>Disable {disableApp_}?</Dialog.Title>
            <Dialog.Description>
                The supervisor stops it, and nothing starts it, not a person, a schedule or the supervisor's own restart, until it is
                enabled again.
            </Dialog.Description>
        </Dialog.Header>
        <div class="space-y-2">
            <Label for="disable-reason">Reason</Label>
            <Input id="disable-reason" maxlength={500} placeholder="Shown to everyone who sees the app" bind:value={disableReason} />
        </div>
        <Dialog.Footer>
            <Button variant="outline" onclick={() => (disabling = false)}>Leave it</Button>
            <Button variant="destructive" disabled={disableReason.trim() === ""} onclick={confirmDisable}>Disable it</Button>
        </Dialog.Footer>
    </Dialog.Content>
</Dialog.Root>
