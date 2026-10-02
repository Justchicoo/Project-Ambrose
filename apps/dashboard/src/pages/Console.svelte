<!-- Project Ambrose by Imjustchico: The remote console shows app output and audited command results; recalled commands come from per-user, per-app history, and the UI displays only the server-redacted command description. While a restore, update, setup or move holds the app its state and progress replace the power controls and only an owner may type. Confirmation and running-state checks remain server-enforced. -->
<script lang="ts">
    import * as Card from "$lib/components/ui/card/index.js";
    import * as Select from "$lib/components/ui/select/index.js";
    import { Button } from "$lib/components/ui/button/index.js";
    import ActivityIcon from "@lucide/svelte/icons/activity";
    import ClockIcon from "@lucide/svelte/icons/clock";
    import EraserIcon from "@lucide/svelte/icons/eraser";
    import GitCommitIcon from "@lucide/svelte/icons/git-commit-horizontal";
    import NetworkIcon from "@lucide/svelte/icons/network";
    import PlayIcon from "@lucide/svelte/icons/play";
    import RotateCcwIcon from "@lucide/svelte/icons/rotate-ccw";
    import SendIcon from "@lucide/svelte/icons/send-horizontal";
    import SquareIcon from "@lucide/svelte/icons/square";
    import { toast } from "svelte-sonner";
    import PageHeader from "../components/PageHeader.svelte";
    import StatusBadge from "../components/StatusBadge.svelte";
    import { focus } from "../focus.svelte";
    import { requestPower } from "../power";
    import { ApiError } from "$lib/api.svelte.js";
    import { formatUptime } from "$lib/format.js";
    import { live } from "$lib/status.svelte.js";
    import { isOwner, may } from "$lib/permission.svelte.js";
    import {
        commandHistory,
        output as capturedOutput,
        heldBy,
        runCommand,
        stateTone,
        stateWord,
        supervised,
    } from "$lib/supervision.svelte.js";
    import type { AppEntry } from "$lib/schemas.js";

    type Line = { kind: "command" | "reply" | "status" | "refused" | "said" | "wrote"; text: string };
    type Tone = "healthy" | "waiting" | "wrong" | "unknown";

    const entries = $derived(supervised());
    const entry = $derived(entries.find((one) => one.name === focus.app) ?? entries[0]);
    const held = $derived(heldBy(entry?.supervision));
    const mayType = $derived(may("console.write", entry?.name) && (!held || isOwner()));
    const mayStart = $derived(may("power.start", entry?.name));
    const mayRestart = $derived(may("power.restart", entry?.name));
    const mayStop = $derived(may("power.stop", entry?.name));

    function appearance(one: AppEntry | undefined): { tone: Tone; word: string } {
        const supervision = one?.supervision;
        if (!supervision) return { tone: "unknown", word: "Not supervised" };
        if (supervision.state === "crashed" && supervision.restart_epoch_ms) return { tone: "waiting", word: "Starting again" };
        return { tone: stateTone(supervision.state), word: stateWord(supervision.state) };
    }

    const app = $derived({
        name: entry?.name ?? "",
        address: entry?.address ?? "",
        port: entry?.port ?? 0,
        revision: entry?.revision ?? "",
        running: entry?.supervision?.state === "running",
        state: appearance(entry).tone,
        word: appearance(entry).word,
        uptime:
            entry?.supervision?.state === "running" && entry.supervision.started_epoch_ms
                ? formatUptime(Math.max(0, Math.round((live.now - entry.supervision.started_epoch_ms) / 1000)))
                : "",
    });

    const outputs = $state<Record<string, Line[]>>({});
    const output = $derived(outputs[app.name] ?? []);
    const seen: Record<string, number> = {};
    let following = $state(true);

    let line = $state("");
    let recall = -1;
    let screen = $state<HTMLDivElement | null>(null);
    let sending = $state(false);
    let awaiting = $state("");

    const stats = $derived([
        {
            icon: NetworkIcon,
            title: "Address",
            value: app.address ? `${app.address}:${app.port}` : "Not listening",
            copy: Boolean(app.address),
            tone: "",
        },
        { icon: ClockIcon, title: "Uptime", value: app.uptime || "Not running", copy: false, tone: "" },
        {
            icon: ActivityIcon,
            title: "State",
            value: app.word,
            copy: false,
            tone: app.state === "wrong" ? "bg-destructive/15 text-destructive" : "",
        },
        { icon: GitCommitIcon, title: "Build", value: app.revision || "Not read", copy: Boolean(app.revision), tone: "" },
    ]);

    const histories = $state<Record<string, string[]>>({});

    function history(name: string): string[] {
        return histories[name] ?? [];
    }

    async function refreshHistory(name: string, signal?: AbortSignal) {
        try {
            histories[name] = (await commandHistory(name, signal)).commands;
        } catch (problem) {
            if (problem instanceof DOMException && problem.name === "AbortError") return;
            const reason = problem instanceof ApiError ? problem.message : String(problem);
            say(name, [{ kind: "status", text: `Command history could not be read: ${reason}` }]);
        }
    }

    function say(name: string, lines: Line[]) {
        outputs[name] = [...(outputs[name] ?? []), ...lines].slice(-2000);
    }

    $effect(() => {
        const name = app.name;
        const beat = live.now;
        void beat;
        if (name === "") return;
        const controller = new AbortController();
        void (async () => {
            try {
                const answer = await capturedOutput(name, "current", controller.signal);
                const after = seen[name] ?? 0;
                const fresh = answer.lines.filter((one) => one.seq > after);
                if (fresh.length > 0) {
                    seen[name] = fresh[fresh.length - 1].seq;
                    say(
                        name,
                        fresh.map((one) => ({ kind: one.stream === "err" ? "wrote" : "said", text: one.text }) as Line),
                    );
                }
            } catch (problem) {
                void problem;
            }
        })();
        return () => controller.abort();
    });

    $effect(() => {
        const name = app.name;
        if (name === "") return;
        const controller = new AbortController();
        void refreshHistory(name, controller.signal);
        return () => controller.abort();
    });

    $effect(() => {
        void output.length;
        if (!screen || !following) return;
        screen.scrollTop = screen.scrollHeight;
    });

    function watchScroll() {
        if (!screen) return;
        following = screen.scrollHeight - screen.scrollTop - screen.clientHeight < 24;
    }

    function asksToConfirm(body: unknown) {
        return body !== null && typeof body === "object" && (body as Record<string, unknown>).needs_confirm === true;
    }

    async function run(command: string, confirm: boolean) {
        const name = app.name;
        sending = true;
        try {
            const answer = await runCommand(name, command, confirm);
            await refreshHistory(name);
            say(name, [{ kind: "command", text: answer.command }]);
            awaiting = answer.needs_confirm ? command : "";
            if (answer.lines.length > 0)
                say(
                    name,
                    answer.lines.map((text) => ({ kind: "reply" as const, text })),
                );
            if (!answer.success) say(name, [{ kind: "refused", text: answer.reason }]);
        } catch (failure) {
            const problem = failure instanceof ApiError ? failure.message : String(failure);
            if (failure instanceof ApiError && failure.body !== null && typeof failure.body === "object") {
                const description = (failure.body as Record<string, unknown>).command;
                if (typeof description === "string") say(name, [{ kind: "command", text: description }]);
            }
            say(name, [{ kind: "refused", text: problem }]);
            awaiting = failure instanceof ApiError && asksToConfirm(failure.body) ? command : "";
            if (awaiting !== "") say(name, [{ kind: "refused", text: "Type yes to run it, or anything else to leave it alone" }]);
        } finally {
            sending = false;
        }
    }

    function send(event: SubmitEvent) {
        event.preventDefault();
        const command = line.trim();
        if (command === "" || sending) return;
        const name = app.name;
        if (awaiting !== "") {
            const held = awaiting;
            line = "";
            recall = -1;
            awaiting = "";
            if (command === "yes" || command === "confirm") {
                void run(held, true);
                return;
            }
            say(name, [{ kind: "refused", text: `${held} was left alone` }]);
            return;
        }
        line = "";
        recall = -1;
        awaiting = command;
        void run(command, false);
    }

    function browse(event: KeyboardEvent) {
        if (event.key !== "ArrowUp" && event.key !== "ArrowDown") return;
        const past = history(app.name);
        if (past.length === 0) return;
        event.preventDefault();
        recall = event.key === "ArrowUp" ? Math.min(recall + 1, past.length - 1) : Math.max(recall - 1, -1);
        line = recall === -1 ? "" : past[recall];
    }

    async function copy(text: string) {
        try {
            await navigator.clipboard.writeText(text);
            toast.success(`Copied ${text}`);
        } catch {
            toast.error("The browser did not allow copying");
        }
    }

    $effect(() => {
        if (screen && output.length > 0) screen.scrollTop = screen.scrollHeight;
    });
</script>

<PageHeader title="Console" description="Run server commands without opening the machine's terminal.">
    {#snippet actions()}
        <Select.Root type="single" bind:value={focus.app} onValueChange={() => (recall = -1)}>
            <Select.Trigger class="w-44" aria-label="App">{focus.app}</Select.Trigger>
            <Select.Content>
                {#each entries as one (one.name)}<Select.Item value={one.name} label={one.name} />{/each}
            </Select.Content>
        </Select.Root>
        {#if held}
            <span class="text-sm text-muted-foreground"
                >{stateWord(held.state)} for {held.holder}{held.progress ? `: ${held.progress}` : ""}, so power waits</span
            >
        {:else if entry?.supervision?.state === "disabled"}
            <span class="text-sm text-muted-foreground">Disabled, so it stays off until it is enabled on the Servers page</span>
        {:else if !app.running}
            {#if mayStart}<Button onclick={() => requestPower("start", app.name)}><PlayIcon />Start</Button>{/if}
        {:else}
            {#if mayRestart}<Button variant="outline" onclick={() => requestPower("restart", app.name)}><RotateCcwIcon />Restart</Button
                >{/if}
            {#if mayStop}
                <Button variant="outline" class="text-destructive hover:text-destructive" onclick={() => requestPower("stop", app.name)}
                    ><SquareIcon />Stop</Button
                >
            {/if}
        {/if}
    {/snippet}
</PageHeader>

<div class="grid gap-4 @4xl/main:grid-cols-4">
    <Card.Root class="gap-0 overflow-hidden py-0 shadow-xs @4xl/main:col-span-3">
        <div class="flex items-center gap-3 border-b bg-sidebar px-4 py-2">
            <span class="font-mono text-xs text-muted-foreground">{app.name}</span>
            <StatusBadge tone={app.state} pulse={app.state === "healthy"}>{app.word}</StatusBadge>
            <Button variant="ghost" size="sm" class="ml-auto h-7 text-muted-foreground" onclick={() => (outputs[app.name] = [])}
                ><EraserIcon />Clear</Button
            >
        </div>
        <div
            bind:this={screen}
            class="h-[52vh] overflow-y-auto bg-sidebar/60 p-4 font-mono text-sm leading-6"
            onscroll={watchScroll}
            role="log"
            aria-label={`Output from ${app.name}`}
        >
            {#each output as entry, index (index)}
                {#if entry.kind === "command"}
                    <div class="flex gap-2 text-foreground">
                        <span class="text-primary select-none">›</span><span class="break-all">{entry.text}</span>
                    </div>
                {:else if entry.kind === "refused"}
                    <div class="pl-4 break-words whitespace-pre-wrap text-destructive">{entry.text}</div>
                {:else if entry.kind === "status"}
                    <div class="text-xs text-muted-foreground italic">{entry.text}</div>
                {:else if entry.kind === "said"}
                    <div class="break-words whitespace-pre-wrap text-foreground/90">{entry.text}</div>
                {:else if entry.kind === "wrote"}
                    <div class="break-words whitespace-pre-wrap text-destructive">{entry.text}</div>
                {:else}
                    <div class="pl-4 break-words whitespace-pre-wrap text-muted-foreground">{entry.text}</div>
                {/if}
            {:else}
                <p class="font-sans text-sm text-muted-foreground">
                    {app.running
                        ? `Waiting for ${app.name} to say something. Everything it prints appears here, and you can type a command below, like status.`
                        : `${app.name} is not running, so it is saying nothing. Its last run's output is on the Servers page, and starting it brings this back to life.`}
                </p>
            {/each}
        </div>
        {#if mayType}
            <form class="flex items-center gap-2 border-t bg-sidebar/60 px-4 py-2" onsubmit={send}>
                <span class="font-mono text-primary select-none" aria-hidden="true">›</span>
                <input
                    bind:value={line}
                    onkeydown={browse}
                    placeholder={!app.running
                        ? `${app.name} is not running`
                        : "Type a command, like status. Up and down recall earlier ones."}
                    disabled={!app.running || sending}
                    class="h-9 min-w-0 flex-1 bg-transparent font-mono text-sm outline-none placeholder:text-muted-foreground disabled:cursor-not-allowed"
                    aria-label="Command"
                    autocomplete="off"
                    spellcheck="false"
                />
                <Button type="submit" size="sm" disabled={!app.running || sending || line.trim() === ""}><SendIcon />Send</Button>
            </form>
        {:else}
            <p class="border-t bg-sidebar/60 px-4 py-3 text-sm text-muted-foreground">
                You can read {app.name}'s output here. Sending it a command needs console.write on this app.
            </p>
        {/if}
    </Card.Root>

    <div class="grid content-start gap-3 @md/main:grid-cols-2 @4xl/main:grid-cols-1">
        {#each stats as stat (stat.title)}
            <Card.Root class="gap-0 py-0 shadow-xs">
                <div class="flex items-center gap-3 p-3">
                    <div
                        class={`flex size-10 shrink-0 items-center justify-center rounded-lg ${stat.tone || "bg-muted text-muted-foreground"}`}
                    >
                        <stat.icon class="size-5" />
                    </div>
                    <div class="min-w-0 flex-1">
                        <div class="text-xs text-muted-foreground">{stat.title}</div>
                        {#if stat.copy}
                            <button
                                type="button"
                                class="truncate font-mono text-sm font-medium hover:underline"
                                title="Copy"
                                onclick={() => copy(stat.value)}>{stat.value}</button
                            >
                        {:else}
                            <div class="truncate text-sm font-medium tabular-nums">{stat.value}</div>
                        {/if}
                    </div>
                </div>
            </Card.Root>
        {/each}
    </div>
</div>
