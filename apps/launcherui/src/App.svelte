<!-- Project Ambrose by Imjustchico: The launcher window as a player meets it, shaped like a game launcher and built for a 1024 by 640 window that never scrolls: a header naming the product and, read-only, the server this run joins and the account slot the server list will fill, with Settings, or Back while settings is open; a hero of our own night sky with the one thing to say in display type over it; a bar beneath carrying what this run is, the server, the client, the window size the client starts at and the language, beside the server's status as a dot and a word and the one gold button, which settings hides since its own Use these is that screen's one action; and a progress strip at the foot with the current step, how many of the first run's steps are done and the percentage, beside the guarantee. Which button shows and what it says, Locate, Checking, Setting up, Play, Launching, Playing or Retry, is the state the launcher answered, never one the page worked out, and when Play cannot be pressed the reason sits beside it. While the launcher sets up, its steps show as a numbered list, each done, active, waiting or failed in a word as well as a colour, and the copy says truthfully that the checks run every time the launcher opens and keep waiting while the server is silent. A failure says its cause in plain words and offers Retry, Open log folder, which asks the launcher to open the folder it keeps the client's log in, and Copy diagnostics, which uses the clipboard and, where the clipboard is refused, shows the text selected so it can be copied by hand. Settings names the installation the launcher found above a two-column form, Locate opens it at the installation field, and saving looks again from the first step with the new values. Every control carries its label and is reached from the keyboard alone. There is no account field: the client's own automatic login is answered by a milestone that is not built, and a box that looks like it signs somebody in and does not is worse than no box. There is no news panel either, because no server feed exists and news the page made up would be a lie. -->
<script lang="ts">
    import { tick } from "svelte";
    import {
        AppShell,
        Button,
        createHost,
        Heading,
        Icon,
        IconButton,
        Mono,
        ProgressBar,
        StateDot,
        TextField,
        VisuallyHidden,
    } from "@ambrose/ui";
    import type { IconName } from "@ambrose/ui";
    import { hostChannel, type SetupStep } from "./lib/channel";
    import { LauncherState, Started } from "./lib/launcher.svelte";
    import NightSky from "./lib/NightSky.svelte";

    const { launcher = new LauncherState(hostChannel(createHost())) }: { launcher?: LauncherState } = $props();

    let client = $state("");
    let host = $state("");
    let port = $state("");
    let locale = $state("");
    let size = $state("");
    let copied = $state("");
    let byHand = $state(false);
    let box: HTMLTextAreaElement | undefined = $state();

    let looked = false;
    $effect(() => {
        if (looked) return;
        looked = true;
        void launcher.run();
    });

    type Primary = { label: string; icon?: IconName; enabled: boolean; act?: () => void };

    const offline = $derived(launcher.server.status === "offline");

    const primary: Primary = $derived.by(() => {
        switch (launcher.state) {
            case "locate":
                return { label: "Locate", icon: "folder", enabled: true, act: () => void locate() };
            case "checking":
                return { label: "Checking", enabled: false };
            case "setting-up":
                return { label: "Setting up", enabled: false };
            case "play":
                return { label: "Play", icon: "play", enabled: !offline, act: () => void launcher.play() };
            case "launching":
                return { label: "Launching", enabled: false };
            case "playing":
                return { label: "Playing", enabled: false };
            default:
                return { label: "Retry", icon: "refresh-cw", enabled: true, act: () => void launcher.again() };
        }
    });

    const why = $derived(
        launcher.state === "locate"
            ? "No installation found"
            : launcher.state === "retry"
              ? `Stopped at: ${launcher.current?.label ?? "the launcher's answer"}`
              : offline
                ? "The login server is not answering"
                : launcher.state === "checking" || launcher.state === "setting-up"
                  ? "Waiting for the checks to finish"
                  : launcher.state === "launching"
                    ? "Handing the game its own window"
                    : launcher.state === "playing"
                      ? "Started, and not watched by Ambrose"
                      : "",
    );

    const chip = $derived(
        launcher.server.status === "online"
            ? { state: "healthy" as const, word: "Online" }
            : launcher.server.status === "offline"
              ? { state: "wrong" as const, word: "Offline" }
              : { state: "unknown" as const, word: "Unknown" },
    );

    const failing = $derived(launcher.current?.state === "wrong" ? launcher.current : null);

    const eyebrow = $derived(
        launcher.screen === "settings"
            ? "Settings"
            : launcher.failed
              ? "Not ready"
              : launcher.state === "checking" || launcher.state === "setting-up"
                ? "Getting ready"
                : launcher.state === "play"
                  ? "Ready"
                  : "Started",
    );

    const headline = $derived(
        launcher.screen === "settings"
            ? "Where the launcher points"
            : launcher.state === "locate"
              ? "Your game installation was not found"
              : launcher.state === "retry"
                ? failing?.id === "server"
                    ? "The login server did not answer"
                    : failing?.id === "run-folder"
                      ? "The game's folder could not be written"
                      : "Not ready yet"
                : launcher.state === "checking"
                  ? "Looking at your installation"
                  : launcher.state === "setting-up"
                    ? "Getting ready to play"
                    : launcher.state === "launching"
                      ? "Starting the game"
                      : launcher.state === "playing"
                        ? "The game was started"
                        : "Ready to play",
    );

    const beneath = $derived(
        launcher.screen === "settings"
            ? "Anything left empty stays as the launcher decided it. Saving looks again from the first step."
            : launcher.state === "play" && launcher.plan
              ? `Your own Wizard101 ${launcher.plan.revision}, started from a folder of its own against ${launcher.plan.host}:${launcher.plan.port}.`
              : launcher.state === "checking" || launcher.state === "setting-up"
                ? "Every time it opens, Ambrose checks your installation, readies the folder the game runs from and asks the login server whether it is there. It keeps asking while the server does not answer."
                : launcher.state === "launching"
                  ? "The game opens in a window of its own."
                  : launcher.state === "playing"
                    ? "It runs in its own window, and closing the launcher leaves it running. Ambrose does not watch the game, so check again to play once more."
                    : "",
    );

    const phase = $derived(
        launcher.failed
            ? "Failed"
            : launcher.state === "checking"
              ? "Checking"
              : launcher.state === "setting-up"
                ? "Setting up"
                : launcher.state === "launching"
                  ? "Launching"
                  : launcher.state === "playing"
                    ? "Playing"
                    : "Ready",
    );

    const total = $derived(launcher.steps.length);
    const percent = $derived(total > 0 ? Math.round((launcher.done / total) * 100) : 0);
    const strip = $derived(
        total === 0
            ? phase
            : launcher.failed && failing
              ? `${phase} · step ${launcher.steps.indexOf(failing) + 1} of ${total}: ${failing.label}`
              : launcher.moving && launcher.current
                ? `${phase} · ${launcher.done} of ${total}: ${launcher.current.label}`
                : `${phase} · ${launcher.done} of ${total}`,
    );

    const showSteps = $derived(launcher.screen === "home" && (launcher.state === "checking" || launcher.state === "setting-up"));

    function marked(step: SetupStep) {
        return step.state === "done"
            ? { state: "healthy" as const, word: "Done" }
            : step.state === "doing"
              ? { state: "waiting" as const, word: "Active" }
              : step.state === "wrong"
                ? { state: "wrong" as const, word: "Failed" }
                : { state: "unknown" as const, word: "Waiting" };
    }

    async function locate() {
        launcher.open("settings");
        await tick();
        document.getElementById("launcher-client")?.focus();
    }

    async function copyDiagnostics() {
        const text = launcher.diagnostics;
        try {
            if (!navigator.clipboard) throw new Error("no clipboard");
            await navigator.clipboard.writeText(text);
            byHand = false;
            copied = "Diagnostics copied.";
        } catch {
            byHand = true;
            copied = "The clipboard was refused here, so the diagnostics are selected below. Press Ctrl+C to copy them.";
            await tick();
            box?.focus();
            box?.select();
        }
    }

    async function saveSettings(event: Event) {
        event.preventDefault();
        copied = "";
        byHand = false;
        await launcher.apply({
            client_dir: client === "" ? undefined : client,
            host: host === "" ? undefined : host,
            port: port === "" ? undefined : port,
            locale: locale === "" ? undefined : locale,
            window: size === "" ? undefined : size,
        });
    }
</script>

<AppShell product="Ambrose" bleed>
    {#snippet barStart()}
        <dl class="flex items-center gap-16 border-l border-edge-quiet pl-12 text-12 text-fg-muted">
            <div class="flex items-center gap-6">
                <dt><Icon name="server" size="13" /><VisuallyHidden>Server</VisuallyHidden></dt>
                <dd>
                    {#if launcher.profile.server}
                        <Mono size="12" tone="muted">{launcher.profile.server.name}</Mono>
                    {:else}
                        No server yet
                    {/if}
                </dd>
            </div>
            <div class="flex items-center gap-6">
                <dt><Icon name="user" size="13" /><VisuallyHidden>Account</VisuallyHidden></dt>
                <dd>{launcher.profile.account ?? "No account yet"}</dd>
            </div>
        </dl>
    {/snippet}

    {#snippet barEnd()}
        {#if launcher.screen === "settings"}
            <Button variant="quiet" icon="arrow-left" onclick={() => launcher.close()}>Back</Button>
        {:else}
            <IconButton icon="settings" label="Settings" onclick={() => launcher.open("settings")} />
        {/if}
    {/snippet}

    <section class="relative flex min-h-0 flex-1 flex-col justify-end overflow-hidden px-40 pb-28 pt-20">
        {#if launcher.screen === "home"}
            <NightSky />
        {/if}

        <div class="relative flex flex-col gap-10" style="max-width: 46rem">
            <p class="ambrose-label text-fg-faint">{eyebrow}</p>
            <Heading level={1} size={launcher.screen === "home" && Started.includes(launcher.state) ? "44" : "34"}>{headline}</Heading>
            {#if beneath !== ""}
                <p class="text-15 text-fg-muted" style="max-width: 72ch">{beneath}</p>
            {/if}

            {#if launcher.screen === "settings"}
                <div class="mt-6 flex flex-col gap-14">
                    <dl class="flex flex-col gap-4">
                        <dt class="ambrose-label text-fg-faint">Installation</dt>
                        <dd>
                            {#if launcher.plan}
                                <Mono>{launcher.plan.install}</Mono>
                            {:else}
                                <span class="text-13 text-fg-muted">None found yet</span>
                            {/if}
                        </dd>
                    </dl>
                    <form class="grid grid-cols-4 items-end gap-x-16 gap-y-14" onsubmit={saveSettings}>
                        <TextField
                            id="launcher-client"
                            label="Installation folder"
                            class="col-span-2"
                            bind:value={client}
                            placeholder={launcher.plan?.install ?? "The folder holding Bin and Data"}
                        />
                        <TextField
                            id="launcher-host"
                            label="Login server"
                            bind:value={host}
                            placeholder={launcher.plan?.host ?? "127.0.0.1"}
                        />
                        <TextField id="launcher-port" label="Port" bind:value={port} placeholder={String(launcher.plan?.port ?? 12000)} />
                        <TextField
                            id="launcher-locale"
                            label="Language"
                            bind:value={locale}
                            placeholder={launcher.plan?.locale ?? "en-US"}
                        />
                        <TextField
                            id="launcher-size"
                            label="Window size"
                            bind:value={size}
                            placeholder={launcher.plan?.window ?? "1280x720"}
                        />
                        <div class="col-span-2 flex justify-end">
                            <Button variant="action" type="submit">Use these</Button>
                        </div>
                    </form>
                </div>
            {:else if showSteps}
                <ol aria-label="First run" class="mt-6 flex list-none flex-col gap-6 p-0" style="max-width: 40rem">
                    {#each launcher.steps as step, index (step.id)}
                        <li class="flex items-start gap-12 rounded-input bg-surface-card/85 px-12 py-8 ring-1 ring-inset ring-edge-quiet">
                            <span
                                class="ambrose-mono flex h-24 w-24 shrink-0 items-center justify-center rounded-pill text-12 ring-1 ring-inset ring-edge-strong"
                                aria-hidden="true">{index + 1}</span
                            >
                            <div class="flex min-w-0 flex-1 flex-col gap-2">
                                <div class="flex items-center justify-between gap-12">
                                    <span class="text-13 text-fg-body"><VisuallyHidden>Step {index + 1}:</VisuallyHidden> {step.label}</span
                                    >
                                    <StateDot state={marked(step).state} word={marked(step).word} size="11" live={step.state === "doing"} />
                                </div>
                                <p class="text-12 text-fg-muted">{step.word}</p>
                            </div>
                        </li>
                    {/each}
                </ol>
            {:else if launcher.failed}
                <div class="mt-6 flex flex-col gap-12">
                    <p class="rounded-input bg-surface-card/85 px-14 py-10 text-15 text-fg-body ring-1 ring-inset ring-state-wrong/40">
                        {launcher.reason === "" ? "The launcher did not say why." : launcher.reason}
                    </p>
                    <div class="flex flex-wrap items-center gap-12">
                        <Button variant="quiet" icon="folder" onclick={() => void launcher.openLogs()}>Open log folder</Button>
                        <Button variant="quiet" icon="copy" onclick={() => void copyDiagnostics()}>Copy diagnostics</Button>
                    </div>
                    <p class="text-12 text-fg-muted" aria-live="polite">
                        {[launcher.logs, copied].filter((note) => note !== "").join(" ")}
                    </p>
                    {#if byHand}
                        <div class="flex flex-col gap-6">
                            <label for="launcher-diagnostics" class="ambrose-label text-fg-faint">Diagnostics</label>
                            <textarea
                                id="launcher-diagnostics"
                                bind:this={box}
                                readonly
                                rows="3"
                                class="ambrose-mono w-full resize-none rounded-input bg-surface-sunken px-12 py-8 text-12 text-fg-body ring-1 ring-inset ring-edge-control"
                                value={launcher.diagnostics}
                            ></textarea>
                        </div>
                    {/if}
                </div>
            {:else if launcher.state === "playing"}
                <div class="mt-6">
                    <Button variant="quiet" icon="rotate-ccw" onclick={() => void launcher.again()}>Check again</Button>
                </div>
            {/if}
        </div>
    </section>

    <footer class="flex shrink-0 items-center justify-between gap-20 border-t border-edge-quiet bg-surface-chrome px-40 py-14">
        <div class="flex min-w-0 flex-wrap items-baseline gap-28">
            {#if launcher.plan}
                <div class="flex flex-col gap-4">
                    <span class="ambrose-label text-fg-faint">Server</span>
                    <Mono>{launcher.plan.host}:{launcher.plan.port}</Mono>
                </div>
                <div class="flex flex-col gap-4">
                    <span class="ambrose-label text-fg-faint">Client</span>
                    <Mono>{launcher.plan.revision}</Mono>
                </div>
                <div class="flex flex-col gap-4">
                    <span class="ambrose-label text-fg-faint">Window</span>
                    <Mono>{launcher.plan.window}</Mono>
                </div>
                <div class="flex flex-col gap-4">
                    <span class="ambrose-label text-fg-faint">Language</span>
                    <Mono>{launcher.plan.locale}</Mono>
                </div>
            {/if}
        </div>

        <div class="flex shrink-0 items-center gap-20">
            <div class="flex flex-col items-end gap-4">
                <span class="ambrose-label text-fg-faint">Login server</span>
                <span role="status"><StateDot state={chip.state} word={chip.word} /></span>
                {#if why !== ""}
                    <span id="launcher-why" class="text-11 text-fg-faint">{why}</span>
                {/if}
            </div>
            {#if launcher.screen === "home"}
                <Button
                    variant="action"
                    size="wide"
                    icon={primary.icon}
                    disabled={!primary.enabled}
                    busy={launcher.starting}
                    busyLabel="Launching"
                    aria-describedby={why !== "" ? "launcher-why" : undefined}
                    onclick={() => primary.act?.()}
                    style="min-width: 204px"
                >
                    {primary.label}
                </Button>
            {/if}
        </div>
    </footer>

    <div class="flex shrink-0 items-center gap-28 border-t border-edge-quiet bg-surface-chrome px-40 py-8">
        <ProgressBar
            class="min-w-0 flex-1"
            label={strip}
            value={launcher.done}
            max={Math.max(total, 1)}
            detail={`${percent}%`}
            tone={launcher.failed ? "wrong" : total > 0 && launcher.done === total ? "healthy" : "action"}
        />
        <p class="shrink-0 text-11 text-fg-faint" style="max-width: 44ch">{launcher.guarantee}</p>
    </div>
</AppShell>
