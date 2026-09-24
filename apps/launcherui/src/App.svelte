<!-- Project Ambrose by Imjustchico: The launcher window: which screen shows is decided by what the launcher answered, not by the page, so ready to play, the first run, and the failure each follow from the answer and settings is the one screen the operator opens themselves. Every screen is reachable from the keyboard alone and every control carries its label, because a launcher somebody cannot use without a mouse is a launcher somebody cannot use. The guarantee sits at the foot of every screen, since what this program will not do to an installation is the thing a player most wants to know before pressing Play. -->
<script lang="ts">
    import { AppShell, Button, Card, createHost, Heading, Label, Mono, StateDot, StepList, TextField } from "@ambrose/ui";
    import { hostChannel } from "./lib/channel";
    import { LauncherState } from "./lib/launcher.svelte";

    const { launcher = new LauncherState(hostChannel(createHost())) }: { launcher?: LauncherState } = $props();

    let host = $state("");
    let port = $state("");
    let locale = $state("");
    let size = $state("");

    $effect(() => {
        void launcher.look();
    });

    async function saveSettings(event: Event) {
        event.preventDefault();
        await launcher.apply({
            host: host === "" ? undefined : host,
            port: port === "" ? undefined : port,
            locale: locale === "" ? undefined : locale,
            window: size === "" ? undefined : size,
        });
    }

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

    async function play(event: Event) {
        event.preventDefault();
        await launcher.play();
    }
</script>

<AppShell product="Ambrose">
    <div class="flex flex-col gap-20">
        {#if launcher.screen === "reading"}
            <Heading level={1}>Looking at your installation</Heading>
            <Card title="Please wait">
                <StateDot state="waiting" word="Reading what is on this machine" />
            </Card>
        {:else if launcher.screen === "first-run"}
            <Heading level={1}>Setting up for the first time</Heading>
            <Card title="First run">
                <StepList label="First run" steps={drawnSteps} />
            </Card>
        {:else if launcher.screen === "failed"}
            <Heading level={1}>Not ready yet</Heading>
            <Card title="What went wrong">
                <StateDot state="wrong" word={launcher.reason} />
                <p class="mt-12 text-13 text-fg-body">
                    Nothing has been changed on this machine. Open Settings to point the launcher at your installation, or start the server
                    and look again.
                </p>
            </Card>
            <div class="flex items-center gap-12">
                <Button variant="quiet" onclick={() => launcher.open("settings")}>Settings</Button>
                <Button variant="action" onclick={() => launcher.look()}>Look again</Button>
            </div>
        {:else if launcher.screen === "settings"}
            <Heading level={1}>Settings</Heading>
            <form class="flex flex-col gap-16" onsubmit={saveSettings}>
                <TextField id="launcher-host" label="Login server" bind:value={host} placeholder={launcher.plan?.host ?? "127.0.0.1"} />
                <TextField id="launcher-port" label="Port" bind:value={port} placeholder={String(launcher.plan?.port ?? 12000)} />
                <TextField id="launcher-locale" label="Language" bind:value={locale} placeholder={launcher.plan?.locale ?? "en-US"} />
                <TextField id="launcher-size" label="Window size" bind:value={size} placeholder="1280x720" />
                <div class="flex items-center gap-12">
                    <Button variant="quiet" type="button" onclick={() => launcher.close()}>Back</Button>
                    <Button variant="action" type="submit">Use these</Button>
                </div>
            </form>
        {:else}
            <Heading level={1}>Ready when you are</Heading>
            <div class="grid grid-cols-3 gap-16">
                <Card title="Server">
                    <StateDot state="healthy" word={`${launcher.plan?.host ?? ""}:${launcher.plan?.port ?? 0}`} />
                </Card>
                <Card title="Client">
                    <StateDot state="healthy" word={launcher.plan?.revision ?? ""} />
                </Card>
                <Card title="Runs in">
                    <Mono>{launcher.plan?.run_folder ?? ""}</Mono>
                </Card>
            </div>
            <form class="flex flex-col gap-16" onsubmit={play}>
                <TextField id="launcher-account" label="Account" bind:value={launcher.account} autocomplete="username" />
                <TextField
                    id="launcher-password"
                    label="Password"
                    type="password"
                    bind:value={launcher.password}
                    autocomplete="current-password"
                />
                <div class="flex items-center gap-12">
                    <Button variant="quiet" type="button" onclick={() => launcher.open("settings")}>Settings</Button>
                    <Button variant="action" size="wide" icon="play" type="submit" disabled={launcher.starting}>
                        {launcher.starting ? "Starting" : "Play"}
                    </Button>
                </div>
            </form>
            <Card title="What will run">
                <Label>Command</Label>
                <Mono>{launcher.plan?.command ?? ""}</Mono>
            </Card>
        {/if}
        <p class="text-12 text-fg-faint">{launcher.guarantee}</p>
    </div>
</AppShell>
