<!-- Project Ambrose by Imjustchico: The configuration page, live from each app: its live settings by category with search, each with its value, default, the layer it comes from, when a change takes hold and whether it is secret, restricted or locked by a layer above live values, changed through a dialog with a typed input, a reason and a review step, with its history and a revert, a reset to the config value, and a reveal for a secret to a caller allowed to see one; presets of chosen categories or keys exported to a file and imported with a checked diff; the options only the config file holds with where each was read; and every option that takes effect only at the next start, with why. It reads the app's event feed every second, so a change made anywhere shows here within about a second, and every fresh read hides a revealed secret again and drops what another app showed. -->
<script lang="ts">
    import * as Card from "$lib/components/ui/card/index.js";
    import * as Dialog from "$lib/components/ui/dialog/index.js";
    import * as Select from "$lib/components/ui/select/index.js";
    import * as Table from "$lib/components/ui/table/index.js";
    import { Button } from "$lib/components/ui/button/index.js";
    import { Input } from "$lib/components/ui/input/index.js";
    import { Label } from "$lib/components/ui/label/index.js";
    import { ApiError } from "$lib/api.svelte.js";
    import { may } from "$lib/permission.svelte.js";
    import { candidates, eventsAfter, settingsOf } from "$lib/supervision.svelte.js";
    import type { SettingHistoryAnswer, SettingsAnswer } from "$lib/schemas.js";
    import { applyPhrase, categories, clipBytes, isLive, isLocked, layerName, makePreset, matches, type Setting } from "$lib/settings.js";
    import ChevronDownIcon from "@lucide/svelte/icons/chevron-down";
    import DownloadIcon from "@lucide/svelte/icons/download";
    import EyeIcon from "@lucide/svelte/icons/eye";
    import EyeOffIcon from "@lucide/svelte/icons/eye-off";
    import HistoryIcon from "@lucide/svelte/icons/history";
    import PencilIcon from "@lucide/svelte/icons/pencil";
    import RotateCcwIcon from "@lucide/svelte/icons/rotate-ccw";
    import SearchIcon from "@lucide/svelte/icons/search";
    import UploadIcon from "@lucide/svelte/icons/upload";
    import PageHeader from "../components/PageHeader.svelte";
    import PresetImport from "../components/PresetImport.svelte";
    import SettingChange from "../components/SettingChange.svelte";
    import SettingHistory from "../components/SettingHistory.svelte";
    import StatusBadge from "../components/StatusBadge.svelte";

    const EventPoll = 1000;

    const choices = $derived(candidates());
    let chosen = $state("");
    const app = $derived(choices.includes(chosen) ? chosen : (choices[0] ?? ""));

    let answer = $state<SettingsAnswer | null>(null);
    let failure = $state("");
    let notice = $state("");
    let search = $state("");
    let read = $state(0);
    let revealed = $state<Record<string, string>>({});
    let revealing = $state("");
    let trouble = $state("");
    let answeredFor = "";

    $effect(() => {
        const name = app;
        void read;
        if (name === "") return;
        if (name !== answeredFor) {
            answeredFor = name;
            answer = null;
            failure = "";
            trouble = "";
            changeOpen = false;
            historyOpen = false;
            importOpen = false;
            exportOpen = false;
        }
        const controller = new AbortController();
        void (async () => {
            try {
                answer = await settingsOf(name, controller.signal);
                revealed = {};
                failure = "";
            } catch (problem) {
                if (controller.signal.aborted) return;
                answer = null;
                failure = problem instanceof ApiError ? problem.message : "The settings could not be read";
            }
        })();
        return () => controller.abort();
    });

    $effect(() => {
        const name = app;
        if (name === "") return;
        const controller = new AbortController();
        let latest = -1;
        let timer: ReturnType<typeof setTimeout> | undefined;
        const poll = async () => {
            try {
                const events = await eventsAfter(name, Math.max(latest, 0), controller.signal);
                if (latest >= 0 && (events.latest < latest || events.records.some((event) => event.kind === "setting.changed"))) read += 1;
                latest = events.latest;
            } catch (problem) {
                if (controller.signal.aborted) return;
                if (problem instanceof ApiError && problem.status === 404) return;
            }
            timer = setTimeout(() => void poll(), EventPoll);
        };
        void poll();
        return () => {
            controller.abort();
            if (timer !== undefined) clearTimeout(timer);
        };
    });

    const settings = $derived(answer?.settings ?? []);
    const groups = $derived(categories(settings, search));
    const liveCount = $derived(settings.filter(isLive).length);
    const others = $derived(settings.filter((setting) => !isLive(setting) && matches(setting, search)));
    const restarts = $derived(settings.filter((setting) => setting.restart_reason !== null));

    const canEdit = $derived(may("settings.edit", app));
    const canRestricted = $derived(may("settings.edit.restricted", app));
    const canReveal = $derived(may("settings.secrets.read", app));

    function editable(setting: Setting): boolean {
        return canEdit && !isLocked(setting) && (setting.edit !== "restricted" || canRestricted);
    }

    let changeOpen = $state(false);
    let changing = $state<Setting | null>(null);
    let changeMode = $state<"edit" | "reset">("edit");
    let changeValue = $state("");
    let changeReason = $state("");

    function edit(setting: Setting) {
        changing = setting;
        changeMode = "edit";
        changeValue = setting.secret ? "" : setting.value;
        changeReason = "";
        changeOpen = true;
    }

    function reset(setting: Setting) {
        changing = setting;
        changeMode = "reset";
        changeValue = "";
        changeReason = "";
        changeOpen = true;
    }

    let historyOpen = $state(false);
    let historyOf = $state<Setting | null>(null);

    function history(setting: Setting) {
        historyOf = setting;
        historyOpen = true;
    }

    function revert(entry: SettingHistoryAnswer["entries"][number]) {
        if (!historyOf) return;
        historyOpen = false;
        changing = historyOf;
        changeMode = "edit";
        changeValue = entry.old;
        changeReason = clipBytes(`Revert change ${entry.id}${entry.reason ? `, which was for: ${entry.reason}` : ""}`, 255);
        changeOpen = true;
    }

    function done(message: string) {
        notice = message;
        read += 1;
    }

    async function reveal(setting: Setting) {
        revealing = setting.key;
        trouble = "";
        try {
            const shown = await settingsOf(app, undefined, setting.key);
            const entry = shown.settings.find((candidate) => candidate.key === setting.key);
            if (entry?.revealed) revealed = { ...revealed, [setting.key]: entry.value };
            else notice = `${setting.key} stays masked: this account is not allowed to see secrets on ${app}`;
        } catch (problem) {
            trouble = problem instanceof ApiError ? problem.message : `${setting.key} could not be revealed`;
        } finally {
            revealing = "";
        }
    }

    function hide(key: string) {
        const next = { ...revealed };
        delete next[key];
        revealed = next;
    }

    let importOpen = $state(false);
    let exportOpen = $state(false);
    let exporting = $state<string[]>([]);
    let opened = $state<string[]>([]);

    const exportGroups = $derived(
        categories(settings, "")
            .map((group) => ({ name: group.name, keys: group.settings.filter((setting) => !setting.secret).map((setting) => setting.key) }))
            .filter((group) => group.keys.length > 0),
    );

    function openExport() {
        const shown = new Set(groups.flatMap((group) => group.settings.map((setting) => setting.key)));
        exporting = exportGroups.flatMap((group) => group.keys).filter((key) => shown.has(key));
        opened = [];
        exportOpen = true;
    }

    function chooseKeys(keys: string[], on: boolean) {
        exporting = on ? [...new Set([...exporting, ...keys])] : exporting.filter((held) => !keys.includes(held));
    }

    function toggleOpened(name: string) {
        opened = opened.includes(name) ? opened.filter((held) => held !== name) : [...opened, name];
    }

    function partly(on: boolean): (node: HTMLInputElement) => void {
        return (node) => {
            node.indeterminate = on;
        };
    }

    function download() {
        const created = new Date();
        const preset = makePreset(app, settings, exporting, created);
        const blob = new Blob([`${JSON.stringify(preset, null, 2)}\n`], { type: "application/json" });
        const link = document.createElement("a");
        link.href = URL.createObjectURL(blob);
        link.download = `${app}-settings-${created.toISOString().slice(0, 10)}.json`;
        document.body.append(link);
        link.click();
        link.remove();
        URL.revokeObjectURL(link.href);
        exportOpen = false;
        notice = `Exported ${Object.keys(preset.settings).length} setting${Object.keys(preset.settings).length === 1 ? "" : "s"} from ${app}; secrets are never exported`;
    }

    function shownValue(setting: Setting): string {
        const value = revealed[setting.key] ?? setting.value;
        return value === "" ? "—" : value;
    }
</script>

<PageHeader
    title="Configuration"
    description={`The settings ${app === "" ? "this app" : app} runs with, changed live where a setting allows it.`}
>
    {#snippet actions()}
        {#if choices.length > 1}
            <Select.Root type="single" bind:value={chosen}>
                <Select.Trigger class="w-44" aria-label="App whose settings are shown">{app}</Select.Trigger>
                <Select.Content>
                    {#each choices as name (name)}
                        <Select.Item value={name}>{name}</Select.Item>
                    {/each}
                </Select.Content>
            </Select.Root>
        {/if}
        <div class="relative">
            <SearchIcon class="absolute top-2.5 left-2.5 size-4 text-muted-foreground" />
            <Input class="w-56 pl-8" placeholder="Search settings" bind:value={search} aria-label="Search settings" />
        </div>
        {#if liveCount > 0}
            <Button variant="outline" onclick={openExport}><DownloadIcon />Export</Button>
            {#if canEdit}<Button variant="outline" onclick={() => (importOpen = true)}><UploadIcon />Import</Button>{/if}
        {/if}
    {/snippet}
</PageHeader>

{#if notice}
    <p class="rounded-md border border-healthy/30 bg-healthy/5 p-3 text-sm text-healthy" role="status">{notice}</p>
{/if}

{#if trouble}
    <p class="rounded-md border border-destructive/30 bg-destructive/5 p-3 text-sm text-destructive" role="alert">{trouble}</p>
{/if}

{#if failure}
    <Card.Root class="shadow-xs">
        <Card.Header>
            <Card.Title>The settings could not be read</Card.Title>
            <Card.Description>{failure}</Card.Description>
        </Card.Header>
    </Card.Root>
{:else if !answer}
    <p class="text-sm text-muted-foreground">Reading the settings of {app === "" ? "this app" : app}.</p>
{:else}
    <div class="flex flex-wrap items-center gap-2 text-sm text-muted-foreground">
        <span>
            {liveCount} live setting{liveCount === 1 ? "" : "s"} and {settings.length - liveCount} other option{settings.length -
                liveCount ===
            1
                ? ""
                : "s"} from
            <span class="font-mono text-xs">{answer.file}</span>
        </span>
        {#if restarts.length > 0}<StatusBadge tone="waiting">{restarts.length} need a restart</StatusBadge>{/if}
    </div>

    {#if liveCount === 0}
        <Card.Root class="shadow-xs">
            <Card.Header>
                <Card.Title>No live settings</Card.Title>
                <Card.Description
                    >{app} declares no setting that changes while it runs; its options below change in its config file.</Card.Description
                >
            </Card.Header>
        </Card.Root>
    {:else if groups.length === 0}
        <p class="text-sm text-muted-foreground">No live setting matches “{search}”.</p>
    {/if}

    {#each groups as group (group.name)}
        <Card.Root class="gap-0 py-0 shadow-xs">
            <Card.Header class="border-b py-3">
                <Card.Title class="text-base">{group.name}</Card.Title>
            </Card.Header>
            <ul class="divide-y">
                {#each group.settings as setting (setting.key)}
                    <li class="flex flex-col gap-3 px-6 py-4 md:flex-row md:items-start md:justify-between" data-setting={setting.key}>
                        <div class="min-w-0 space-y-1">
                            <div class="flex flex-wrap items-center gap-2">
                                <span class="font-mono text-xs font-medium">{setting.key}</span>
                                {#if setting.secret}<StatusBadge tone="mine">Secret</StatusBadge>{/if}
                                {#if setting.edit === "restricted"}<StatusBadge tone="waiting">Restricted</StatusBadge>{/if}
                                {#if isLocked(setting)}<StatusBadge tone="unknown"
                                        >Locked · {layerName(setting.lock?.layer ?? setting.layer)}</StatusBadge
                                    >{/if}
                            </div>
                            <p class="text-xs text-muted-foreground">{setting.description}</p>
                            <p class="text-xs text-muted-foreground">
                                {applyPhrase(setting)}.{setting.bounds
                                    ? ` ${setting.bounds[0].toUpperCase()}${setting.bounds.slice(1)}.`
                                    : ""}
                                {#if isLocked(setting)}Set by {setting.lock?.origin}, which overrides any live value.{/if}
                            </p>
                        </div>
                        <div class="flex min-w-0 flex-col gap-2 md:max-w-1/2 md:shrink-0 md:items-end">
                            <div class="font-mono text-sm break-all md:text-right">
                                {shownValue(setting)}{setting.unit && setting.value !== "" ? ` ${setting.unit}` : ""}
                            </div>
                            <div class="text-xs text-muted-foreground md:text-right">
                                {layerName(setting.layer)} · default {setting.declared_default === "" ? "empty" : setting.declared_default}
                            </div>
                            <div class="flex flex-wrap gap-1 md:justify-end">
                                {#if editable(setting)}
                                    <Button variant="outline" size="sm" onclick={() => edit(setting)} aria-label={`Change ${setting.key}`}
                                        ><PencilIcon />Change</Button
                                    >
                                    {#if setting.layer === "live"}
                                        <Button variant="ghost" size="sm" onclick={() => reset(setting)} aria-label={`Reset ${setting.key}`}
                                            ><RotateCcwIcon />Reset</Button
                                        >
                                    {/if}
                                {/if}
                                <Button variant="ghost" size="sm" onclick={() => history(setting)} aria-label={`History of ${setting.key}`}
                                    ><HistoryIcon />History</Button
                                >
                                {#if setting.secret && canReveal}
                                    {#if revealed[setting.key] !== undefined}
                                        <Button
                                            variant="ghost"
                                            size="sm"
                                            onclick={() => hide(setting.key)}
                                            aria-label={`Hide ${setting.key}`}><EyeOffIcon />Hide</Button
                                        >
                                    {:else}
                                        <Button
                                            variant="ghost"
                                            size="sm"
                                            disabled={revealing === setting.key}
                                            onclick={() => void reveal(setting)}
                                            aria-label={`Reveal ${setting.key}`}><EyeIcon />Reveal</Button
                                        >
                                    {/if}
                                {/if}
                            </div>
                        </div>
                    </li>
                {/each}
            </ul>
        </Card.Root>
    {/each}

    {#if restarts.length > 0}
        <Card.Root class="shadow-xs">
            <Card.Header>
                <Card.Title class="text-base">Takes effect at the next start</Card.Title>
                <Card.Description>A change to one of these waits for {app} to restart, for the reason given.</Card.Description>
            </Card.Header>
            <Card.Content>
                <ul class="space-y-2 text-sm">
                    {#each restarts as setting (setting.key)}
                        <li>
                            <span class="font-mono text-xs font-medium">{setting.key}</span>
                            <span class="text-muted-foreground">— {setting.restart_reason}</span>
                        </li>
                    {/each}
                </ul>
            </Card.Content>
        </Card.Root>
    {/if}

    {#if others.length > 0}
        <Card.Root class="py-0 shadow-xs">
            <Table.Root>
                <Table.Header>
                    <Table.Row class="hover:bg-transparent">
                        <Table.Head class="pl-6">Config file options</Table.Head>
                        <Table.Head>Value in use</Table.Head>
                        <Table.Head class="hidden lg:table-cell">Shipped default</Table.Head>
                        <Table.Head class="hidden md:table-cell">Read from</Table.Head>
                    </Table.Row>
                </Table.Header>
                <Table.Body>
                    {#each others as setting (setting.key)}
                        <Table.Row>
                            <Table.Cell class="pl-6 align-top">
                                <div class="font-mono text-xs font-medium">{setting.key}</div>
                                <div class="mt-1 flex flex-wrap gap-1">
                                    {#if setting.secret}<StatusBadge tone="mine">Secret</StatusBadge>{/if}
                                    {#if setting.restart_reason}<StatusBadge tone="waiting">Restart required</StatusBadge>{/if}
                                </div>
                            </Table.Cell>
                            <Table.Cell class="align-top font-mono text-xs break-all whitespace-normal"
                                >{setting.value === "" ? "—" : setting.value}</Table.Cell
                            >
                            <Table.Cell class="hidden align-top font-mono text-xs break-all whitespace-normal lg:table-cell"
                                >{setting.default === null ? "Not shipped" : setting.default === "" ? "—" : setting.default}</Table.Cell
                            >
                            <Table.Cell class="hidden align-top text-xs break-all whitespace-normal md:table-cell">
                                <div>{layerName(setting.layer)}</div>
                                <div class="text-muted-foreground">{setting.file}{setting.line > 0 ? `:${setting.line}` : ""}</div>
                            </Table.Cell>
                        </Table.Row>
                    {/each}
                </Table.Body>
            </Table.Root>
        </Card.Root>
    {/if}
{/if}

<SettingChange
    bind:open={changeOpen}
    {app}
    setting={changing}
    mode={changeMode}
    bind:value={changeValue}
    bind:reason={changeReason}
    {done}
/>
<SettingHistory bind:open={historyOpen} {app} setting={historyOf} canChange={historyOf !== null && editable(historyOf)} {revert} />
<PresetImport bind:open={importOpen} {app} {settings} {done} />

<Dialog.Root bind:open={exportOpen}>
    <Dialog.Content>
        <Dialog.Header>
            <Dialog.Title>Export a settings preset from {app}</Dialog.Title>
            <Dialog.Description
                >The chosen live settings go into a file another app or installation can import, whole categories or single keys. Secrets
                are never exported.</Dialog.Description
            >
        </Dialog.Header>
        <div class="max-h-96 space-y-2 overflow-y-auto">
            {#each exportGroups as group (group.name)}
                {@const picked = group.keys.filter((key) => exporting.includes(key)).length}
                <div>
                    <div class="flex items-center gap-2">
                        <input
                            id={`export-${group.name}`}
                            type="checkbox"
                            class="size-4"
                            checked={picked === group.keys.length}
                            {@attach partly(picked > 0 && picked < group.keys.length)}
                            onchange={(event) => chooseKeys(group.keys, event.currentTarget.checked)}
                        />
                        <Label for={`export-${group.name}`}>{group.name} ({picked} of {group.keys.length})</Label>
                        <Button
                            variant="ghost"
                            size="sm"
                            class="ml-auto"
                            aria-expanded={opened.includes(group.name)}
                            aria-label={`Choose keys in ${group.name}`}
                            onclick={() => toggleOpened(group.name)}
                            ><ChevronDownIcon class={opened.includes(group.name) ? "rotate-180" : ""} />Keys</Button
                        >
                    </div>
                    {#if opened.includes(group.name)}
                        <ul class="mt-1 space-y-1 pl-6">
                            {#each group.keys as key (key)}
                                <li class="flex items-center gap-2">
                                    <input
                                        id={`export-key-${key}`}
                                        type="checkbox"
                                        class="size-4 shrink-0"
                                        checked={exporting.includes(key)}
                                        onchange={(event) => chooseKeys([key], event.currentTarget.checked)}
                                    />
                                    <Label for={`export-key-${key}`} class="font-mono text-xs break-all">{key}</Label>
                                </li>
                            {/each}
                        </ul>
                    {/if}
                </div>
            {/each}
        </div>
        <Dialog.Footer>
            <Button variant="outline" onclick={() => (exportOpen = false)}>Leave it</Button>
            <Button disabled={exporting.length === 0} onclick={download}
                ><DownloadIcon />Export {exporting.length} setting{exporting.length === 1 ? "" : "s"}</Button
            >
        </Dialog.Footer>
    </Dialog.Content>
</Dialog.Root>
