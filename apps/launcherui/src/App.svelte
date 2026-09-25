<!-- Project Ambrose by Imjustchico: The launcher window as a player meets it: the night ground filling the frame, one thing said in display type where the eye lands, and beneath it a bar carrying what this run is and the single gold Play. Which screen shows is decided by what the launcher answered, not by the page, so the first run and the failure each follow from the answer, and settings is the one screen the operator opens themselves. The facts on the bar are the settings the launcher decided and are shown as values rather than as states, because this program has not asked the server whether it is there and must not look as though it had. Every screen is reachable from the keyboard alone and every control carries its label. There is no account field: the client's own automatic login is answered by a milestone that is not built, and a box that looks like it signs somebody in and does not is worse than no box. The launcher is asked what it found once when the window opens rather than on every redraw, since each asking reads the install again. -->
<script lang="ts">
    import { AppShell, Button, Card, createHost, Heading, IconButton, Mono, ProgressBar, StateDot, StepList, TextField } from "@ambrose/ui";
    import { hostChannel } from "./lib/channel";
    import { LauncherState } from "./lib/launcher.svelte";

    const { launcher = new LauncherState(hostChannel(createHost())) }: { launcher?: LauncherState } = $props();

    let host = $state("");
    let port = $state("");
    let locale = $state("");
    let size = $state("");

    let looked = false;
    $effect(() => {
        if (looked) return;
        looked = true;
        void launcher.look();
    });

    const drawnSteps = $derived(
        launcher.steps.map((step) => ({
            id: step.id,
            label: step.label,
            state:
                step.state === "done"
                    ? ("healthy" as const)
                    : step.state === "doing"
                      ? ("waiting" as const)
                      : step.state === "wrong"
                        ? ("wrong" as const)
                        : ("unknown" as const),
            word: step.word,
        })),
    );

    const finished = $derived(launcher.steps.filter((step) => step.state === "done").length);

    const eyebrow = $derived(
        launcher.screen === "reading"
            ? "Starting up"
            : launcher.screen === "first-run"
              ? "First run"
              : launcher.screen === "failed"
                ? "Not ready"
                : launcher.screen === "settings"
                  ? "Settings"
                  : "Ready",
    );

    const headline = $derived(
        launcher.screen === "reading"
            ? "Looking at your installation"
            : launcher.screen === "first-run"
              ? "Setting up for the first time"
              : launcher.screen === "failed"
                ? "Not ready yet"
                : launcher.screen === "settings"
                  ? "Where the launcher points"
                  : "Ready to play",
    );

    const beneath = $derived(
        launcher.screen === "ready" && launcher.plan
            ? `Your own Wizard101 ${launcher.plan.revision}, started from a folder of its own against ${launcher.plan.host}:${launcher.plan.port}.`
            : launcher.screen === "failed"
              ? `${launcher.reason}. Nothing on this machine has been changed.`
              : launcher.screen === "first-run"
                ? "This happens once. Everything is read from the installation you already have."
                : launcher.screen === "settings"
                  ? "Anything left empty stays as the launcher decided it."
                  : "",
    );

    async function saveSettings(event: Event) {
        event.preventDefault();
        await launcher.apply({
            host: host === "" ? undefined : host,
            port: port === "" ? undefined : port,
            locale: locale === "" ? undefined : locale,
            window: size === "" ? undefined : size,
        });
    }
</script>

<AppShell product="Ambrose" bleed>
    {#snippet barEnd()}
        {#if launcher.screen === "settings"}
            <Button variant="quiet" onclick={() => launcher.close()}>Back</Button>
        {:else}
            <IconButton icon="settings" label="Settings" onclick={() => launcher.open("settings")} />
        {/if}
    {/snippet}

    <section class="relative flex min-h-0 flex-1 flex-col justify-end overflow-auto p-40" data-ambrose-scroll>
        <div class="sky pointer-events-none absolute inset-0" aria-hidden="true"></div>
        <div class="stars pointer-events-none absolute inset-0" aria-hidden="true"></div>

        <div class="relative flex flex-col gap-14">
            <p class="ambrose-label text-fg-faint">{eyebrow}</p>
            <Heading level={1} size="44">{headline}</Heading>
            {#if beneath !== ""}
                <p class="text-15 text-fg-muted" style="max-width: 58ch">{beneath}</p>
            {/if}

            {#if launcher.screen === "reading"}
                <StateDot state="waiting" word="Reading what is on this machine" />
            {:else if launcher.screen === "first-run"}
                <div class="mt-6" style="max-width: 34rem">
                    <Card title="What is being set up">
                        <StepList label="First run" steps={drawnSteps} />
                        {#if launcher.steps.length > 0}
                            <ProgressBar
                                label="Steps done"
                                value={finished}
                                max={launcher.steps.length}
                                detail={`${finished} of ${launcher.steps.length}`}
                            />
                        {/if}
                    </Card>
                </div>
            {:else if launcher.screen === "settings"}
                <div class="mt-6" style="max-width: 30rem">
                    <Card title="This machine">
                        <form class="flex flex-col gap-16" onsubmit={saveSettings}>
                            <TextField
                                id="launcher-host"
                                label="Login server"
                                bind:value={host}
                                placeholder={launcher.plan?.host ?? "127.0.0.1"}
                            />
                            <TextField
                                id="launcher-port"
                                label="Port"
                                bind:value={port}
                                placeholder={String(launcher.plan?.port ?? 12000)}
                            />
                            <TextField
                                id="launcher-locale"
                                label="Language"
                                bind:value={locale}
                                placeholder={launcher.plan?.locale ?? "en-US"}
                            />
                            <TextField id="launcher-size" label="Window size" bind:value={size} placeholder="1280x720" />
                            <div class="flex items-center gap-12">
                                <Button variant="quiet" type="button" onclick={() => launcher.close()}>Back</Button>
                                <Button variant="action" type="submit">Use these</Button>
                            </div>
                        </form>
                    </Card>
                </div>
            {/if}
        </div>
    </section>

    <footer class="flex shrink-0 flex-wrap items-end justify-between gap-20 border-t border-edge-quiet bg-surface-chrome px-40 py-16">
        <div class="flex flex-col gap-10">
            {#if launcher.plan}
                <div class="flex flex-wrap items-baseline gap-28">
                    <div class="flex flex-col gap-4">
                        <span class="ambrose-label text-fg-faint">Server</span>
                        <Mono>{launcher.plan.host}:{launcher.plan.port}</Mono>
                    </div>
                    <div class="flex flex-col gap-4">
                        <span class="ambrose-label text-fg-faint">Client</span>
                        <Mono>{launcher.plan.revision}</Mono>
                    </div>
                    <div class="flex flex-col gap-4">
                        <span class="ambrose-label text-fg-faint">Language</span>
                        <Mono>{launcher.plan.locale}</Mono>
                    </div>
                </div>
            {/if}
            <p class="text-11 text-fg-faint" style="max-width: 72ch">{launcher.guarantee}</p>
        </div>

        {#if launcher.screen === "ready"}
            <Button
                variant="action"
                size="wide"
                icon="play"
                busy={launcher.starting}
                busyLabel="Starting"
                onclick={() => launcher.play()}
                style="min-width: 204px"
            >
                Play
            </Button>
        {:else if launcher.screen === "failed"}
            <Button variant="action" size="wide" icon="refresh-cw" onclick={() => launcher.look()} style="min-width: 204px">
                Look again
            </Button>
        {/if}
    </footer>
</AppShell>

<style>
    .sky {
        background-image:
            radial-gradient(130% 95% at 50% -30%, var(--color-surface-card), var(--color-transparent) 62%),
            radial-gradient(85% 80% at 88% 120%, var(--color-surface-sunken), var(--color-transparent) 70%);
    }

    .stars {
        opacity: 0.45;
        background-repeat: no-repeat;
        background-image:
            radial-gradient(1.5px 1.5px at 8% 18%, var(--color-fg-body), var(--color-transparent)),
            radial-gradient(1px 1px at 17% 42%, var(--color-fg-muted), var(--color-transparent)),
            radial-gradient(1px 1px at 24% 12%, var(--color-fg-faint), var(--color-transparent)),
            radial-gradient(2px 2px at 31% 28%, var(--color-fg-body), var(--color-transparent)),
            radial-gradient(1px 1px at 39% 8%, var(--color-fg-muted), var(--color-transparent)),
            radial-gradient(1px 1px at 44% 35%, var(--color-fg-faint), var(--color-transparent)),
            radial-gradient(1.5px 1.5px at 52% 16%, var(--color-fg-body), var(--color-transparent)),
            radial-gradient(1px 1px at 58% 46%, var(--color-fg-muted), var(--color-transparent)),
            radial-gradient(1px 1px at 63% 6%, var(--color-fg-faint), var(--color-transparent)),
            radial-gradient(2px 2px at 71% 24%, var(--color-fg-body), var(--color-transparent)),
            radial-gradient(1px 1px at 77% 13%, var(--color-fg-muted), var(--color-transparent)),
            radial-gradient(1px 1px at 82% 39%, var(--color-fg-faint), var(--color-transparent)),
            radial-gradient(1.5px 1.5px at 88% 21%, var(--color-fg-body), var(--color-transparent)),
            radial-gradient(1px 1px at 94% 9%, var(--color-fg-muted), var(--color-transparent)),
            radial-gradient(1px 1px at 12% 62%, var(--color-fg-faint), var(--color-transparent)),
            radial-gradient(1px 1px at 68% 58%, var(--color-fg-faint), var(--color-transparent));
    }
</style>
