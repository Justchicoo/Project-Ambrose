<!-- Project Ambrose by Imjustchico: The launch page, live from the supervisor: how one app is launched, the build it runs, its command-line overrides shown locked with the command-line override layer named, its environment variables with secret values masked, the working folder, start and stop timeouts, the restart policy with its backoff and crash loop limits, autostart, process priority and the opt-in memory and CPU limits, every change previewing the exact command line and environment before it is applied at the app's next start, shown only to a caller allowed to read or change it. -->
<script lang="ts">
    import * as Card from "$lib/components/ui/card/index.js";
    import * as Table from "$lib/components/ui/table/index.js";
    import { Button } from "$lib/components/ui/button/index.js";
    import { Input } from "$lib/components/ui/input/index.js";
    import { Label } from "$lib/components/ui/label/index.js";
    import { ApiError } from "$lib/api.svelte.js";
    import { may } from "$lib/permission.svelte.js";
    import { candidates, launchOf, updateLaunch } from "$lib/supervision.svelte.js";
    import { LaunchAnswer } from "$lib/schemas.js";
    import type { InferOutput } from "valibot";
    import LockIcon from "@lucide/svelte/icons/lock";
    import PageHeader from "../components/PageHeader.svelte";

    type Launch = InferOutput<typeof LaunchAnswer>;

    const choices = $derived(candidates());
    let chosen = $state("");
    let answer = $state<Launch | null>(null);
    let failure = $state("");
    let saving = $state(false);
    let saved = $state("");
    let reason = $state("");

    let build = $state("");
    let workingDirectory = $state("");
    let startTimeout = $state("");
    let stopTimeout = $state("");
    let restartPolicy = $state("");
    let backoffSeconds = $state("");
    let maxRestarts = $state("");
    let crashWindowSeconds = $state("");
    let autostart = $state("");
    let priority = $state("");
    let memoryLimitMb = $state("");
    let cpuLimitPercent = $state("");
    let overridesText = $state("");
    let environmentText = $state("");
    let secretsText = $state("");

    const app = $derived(choices.includes(chosen) ? chosen : (choices[0] ?? ""));
    const canEdit = $derived(may("launch.edit", app));
    const canRead = $derived(may("launch.read", app));

    function textOf(value: unknown): string {
        return value === null || value === undefined ? "" : String(value);
    }

    function fill(form: Launch) {
        build = form.build;
        workingDirectory = form.working_directory;
        startTimeout = textOf(form.start_timeout);
        stopTimeout = textOf(form.stop_timeout);
        restartPolicy = textOf(form.restart_policy);
        backoffSeconds = textOf(form.backoff_seconds);
        maxRestarts = textOf(form.max_restarts);
        crashWindowSeconds = textOf(form.crash_window_seconds);
        autostart = form.autostart === null || form.autostart === undefined ? "" : String(form.autostart);
        priority = textOf(form.priority);
        memoryLimitMb = form.memory_limit_bytes ? String(Math.round(form.memory_limit_bytes / 1048576)) : "";
        cpuLimitPercent = textOf(form.cpu_limit_percent);
        overridesText = Object.entries(form.overrides)
            .map(([key, value]) => `${key}=${value}`)
            .join("\n");
        environmentText = Object.entries(form.environment)
            .map(([name, value]) => `${name}=${value}`)
            .join("\n");
        secretsText = form.secret_environment.join("\n");
        reason = "";
        saved = "";
    }

    $effect(() => {
        const name = app;
        if (name === "") return;
        const controller = new AbortController();
        answer = null;
        failure = "";
        void (async () => {
            try {
                const form = await launchOf(name, controller.signal);
                if (controller.signal.aborted) return;
                answer = form;
                fill(form);
            } catch (problem) {
                if (controller.signal.aborted) return;
                failure = problem instanceof ApiError ? problem.message : "The launch settings could not be read";
            }
        })();
        return () => controller.abort();
    });

    function parsePairs(text: string): Record<string, string> {
        const pairs: Record<string, string> = {};
        for (const line of text.split("\n")) {
            const trimmed = line.trim();
            if (trimmed === "") continue;
            const equals = trimmed.indexOf("=");
            if (equals <= 0) throw new Error(`"${trimmed}" needs a name=value shape`);
            pairs[trimmed.slice(0, equals).trim()] = trimmed.slice(equals + 1);
        }
        return pairs;
    }

    function numberOrNull(text: string): number | null {
        const trimmed = text.trim();
        if (trimmed === "") return null;
        const value = Number(trimmed);
        if (!Number.isFinite(value)) throw new Error(`"${trimmed}" is not a number`);
        return value;
    }

    async function save() {
        if (!canEdit || saving || app === "") return;
        saving = true;
        failure = "";
        saved = "";
        try {
            const settings: Record<string, unknown> = {
                build: build.trim(),
                working_directory: workingDirectory.trim(),
                start_timeout: numberOrNull(startTimeout),
                stop_timeout: numberOrNull(stopTimeout),
                restart_policy: restartPolicy.trim() === "" ? null : restartPolicy.trim(),
                backoff_seconds: numberOrNull(backoffSeconds),
                max_restarts: numberOrNull(maxRestarts),
                crash_window_seconds: numberOrNull(crashWindowSeconds),
                autostart: autostart.trim() === "" ? null : autostart.trim() === "true",
                priority: numberOrNull(priority),
                memory_limit_bytes: memoryLimitMb.trim() === "" ? null : Math.round(Number(memoryLimitMb) * 1048576),
                cpu_limit_percent: numberOrNull(cpuLimitPercent),
                overrides: parsePairs(overridesText),
                environment: parsePairs(environmentText),
                secret_environment: secretsText
                    .split("\n")
                    .map((name) => name.trim())
                    .filter((name) => name !== ""),
            };
            const form = await updateLaunch(app, settings, reason.trim());
            answer = form;
            fill(form);
            saved = `The launch settings for ${app} were saved and apply at its next start`;
        } catch (problem) {
            failure =
                problem instanceof ApiError
                    ? problem.message
                    : problem instanceof Error
                      ? problem.message
                      : "The launch settings could not be saved";
        } finally {
            saving = false;
        }
    }

    const lockedKeys = $derived(answer?.locked_keys ?? []);
    const previewEnvironment = $derived(answer?.preview_environment ?? []);
</script>

<PageHeader title="Launch settings" description="How each app is launched, applied at its next start" />

{#if !canRead}
    <p>You are not allowed to see how apps are launched.</p>
{:else}
    <div class="mb-4 flex items-center gap-2">
        <Label for="launch-app">App</Label>
        <select id="launch-app" class="rounded border px-2 py-1" bind:value={chosen} aria-label="App">
            {#each choices as name (name)}
                <option value={name}>{name}</option>
            {/each}
        </select>
    </div>

    {#if failure !== ""}
        <p role="alert" class="mb-4 text-red-600">{failure}</p>
    {/if}
    {#if saved !== ""}
        <p role="status" class="mb-4 text-green-700">{saved}</p>
    {/if}

    {#if answer === null && failure === ""}
        <p>Reading the launch settings…</p>
    {:else if answer !== null}
        <div class="grid gap-4 md:grid-cols-2">
            <Card.Root>
                <Card.Header><Card.Title>Program</Card.Title></Card.Header>
                <Card.Content class="grid gap-3">
                    <div>
                        <Label for="launch-build">Build</Label>
                        <Input id="launch-build" bind:value={build} disabled={!canEdit} placeholder="Inherit the configured program" />
                    </div>
                    <div>
                        <Label for="launch-workdir">Working folder</Label>
                        <Input
                            id="launch-workdir"
                            bind:value={workingDirectory}
                            disabled={!canEdit}
                            placeholder="Inherit the configured folder"
                        />
                    </div>
                    <div class="grid grid-cols-2 gap-3">
                        <div>
                            <Label for="launch-start-timeout">Start timeout (s)</Label>
                            <Input id="launch-start-timeout" bind:value={startTimeout} disabled={!canEdit} inputmode="numeric" />
                        </div>
                        <div>
                            <Label for="launch-stop-timeout">Stop timeout (s)</Label>
                            <Input id="launch-stop-timeout" bind:value={stopTimeout} disabled={!canEdit} inputmode="numeric" />
                        </div>
                    </div>
                    <div class="grid grid-cols-2 gap-3">
                        <div>
                            <Label for="launch-policy">Restart policy</Label>
                            <select
                                id="launch-policy"
                                class="w-full rounded border px-2 py-1"
                                bind:value={restartPolicy}
                                disabled={!canEdit}
                            >
                                <option value="">Inherit</option>
                                <option value="always">Always</option>
                                <option value="on-crash">On crash</option>
                                <option value="never">Never</option>
                            </select>
                        </div>
                        <div>
                            <Label for="launch-autostart">Autostart</Label>
                            <select
                                id="launch-autostart"
                                class="w-full rounded border px-2 py-1"
                                bind:value={autostart}
                                disabled={!canEdit}
                            >
                                <option value="">Inherit</option>
                                <option value="true">Yes</option>
                                <option value="false">No</option>
                            </select>
                        </div>
                    </div>
                    <div class="grid grid-cols-3 gap-3">
                        <div>
                            <Label for="launch-backoff">Backoff (s)</Label>
                            <Input id="launch-backoff" bind:value={backoffSeconds} disabled={!canEdit} inputmode="numeric" />
                        </div>
                        <div>
                            <Label for="launch-max-restarts">Crash loop limit</Label>
                            <Input id="launch-max-restarts" bind:value={maxRestarts} disabled={!canEdit} inputmode="numeric" />
                        </div>
                        <div>
                            <Label for="launch-crash-window">Crash window (s)</Label>
                            <Input id="launch-crash-window" bind:value={crashWindowSeconds} disabled={!canEdit} inputmode="numeric" />
                        </div>
                    </div>
                    <div class="grid grid-cols-3 gap-3">
                        <div>
                            <Label for="launch-priority">Priority (-2 to 2)</Label>
                            <Input id="launch-priority" bind:value={priority} disabled={!canEdit} inputmode="numeric" />
                        </div>
                        <div>
                            <Label for="launch-memory">Memory limit (MB)</Label>
                            <Input
                                id="launch-memory"
                                bind:value={memoryLimitMb}
                                disabled={!canEdit}
                                inputmode="numeric"
                                placeholder="Off"
                            />
                        </div>
                        <div>
                            <Label for="launch-cpu">CPU limit (%)</Label>
                            <Input id="launch-cpu" bind:value={cpuLimitPercent} disabled={!canEdit} inputmode="numeric" placeholder="Off" />
                        </div>
                    </div>
                </Card.Content>
            </Card.Root>

            <Card.Root>
                <Card.Header><Card.Title>Overrides and environment</Card.Title></Card.Header>
                <Card.Content class="grid gap-3">
                    <div>
                        <Label for="launch-overrides">Command-line overrides, one key=value per line</Label>
                        <textarea
                            id="launch-overrides"
                            class="w-full rounded border px-2 py-1 font-mono text-sm"
                            rows="4"
                            bind:value={overridesText}
                            disabled={!canEdit}
                        ></textarea>
                        {#if lockedKeys.length > 0}
                            <ul class="mt-2 text-sm">
                                {#each lockedKeys as entry (entry.key)}
                                    <li class="flex items-center gap-1">
                                        <LockIcon size={14} />{entry.key} is locked by the {entry.layer} layer
                                    </li>
                                {/each}
                            </ul>
                        {/if}
                    </div>
                    <div>
                        <Label for="launch-env">Environment, one name=value per line</Label>
                        <textarea
                            id="launch-env"
                            class="w-full rounded border px-2 py-1 font-mono text-sm"
                            rows="4"
                            bind:value={environmentText}
                            disabled={!canEdit}
                        ></textarea>
                    </div>
                    <div>
                        <Label for="launch-secrets">Secret variables, one name per line, sealed and masked</Label>
                        <textarea
                            id="launch-secrets"
                            class="w-full rounded border px-2 py-1 font-mono text-sm"
                            rows="2"
                            bind:value={secretsText}
                            disabled={!canEdit}
                        ></textarea>
                    </div>
                </Card.Content>
            </Card.Root>
        </div>

        <Card.Root class="mt-4">
            <Card.Header><Card.Title>Preview</Card.Title></Card.Header>
            <Card.Content>
                <p class="mb-1 text-sm font-medium">Exact command line</p>
                <pre class="mb-4 overflow-x-auto rounded bg-muted p-2 font-mono text-sm">{answer.preview_command_line}</pre>
                {#if previewEnvironment.length > 0}
                    <p class="mb-1 text-sm font-medium">Environment</p>
                    <Table.Root>
                        <Table.Body>
                            {#each previewEnvironment as entry (entry.name)}
                                <Table.Row>
                                    <Table.Cell class="font-mono">{entry.name}</Table.Cell>
                                    <Table.Cell class="font-mono">{entry.value}</Table.Cell>
                                    <Table.Cell>{entry.secret ? "sealed" : ""}</Table.Cell>
                                </Table.Row>
                            {/each}
                        </Table.Body>
                    </Table.Root>
                {/if}
            </Card.Content>
        </Card.Root>

        {#if canEdit}
            <div class="mt-4 flex items-end gap-2">
                <div class="grow">
                    <Label for="launch-reason">Reason for the audit log</Label>
                    <Input id="launch-reason" bind:value={reason} placeholder="Why these launch settings change" />
                </div>
                <Button onclick={save} disabled={saving}>{saving ? "Saving…" : "Save launch settings"}</Button>
            </div>
        {/if}
    {/if}
{/if}
