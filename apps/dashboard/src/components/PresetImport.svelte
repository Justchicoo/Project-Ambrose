<!-- Project Ambrose by Imjustchico: Importing a settings preset: the file is read and every entry checked against the app's own schema, then the entries that would change are checked by the app itself in a dry run, and the diff shows each key's value now and in the preset with whether it changes, already holds it, or is refused and why; nothing is applied until the app's dry run of this same file has passed with no entry refused, a dry run that could not be made is offered again, and applying sends every change as one batch the app takes whole or not at all. Keys the preset does not name are never touched. -->
<script lang="ts">
    import * as Dialog from "$lib/components/ui/dialog/index.js";
    import * as Table from "$lib/components/ui/table/index.js";
    import { Button } from "$lib/components/ui/button/index.js";
    import { Label } from "$lib/components/ui/label/index.js";
    import { Textarea } from "$lib/components/ui/textarea/index.js";
    import { ApiError } from "$lib/api.svelte.js";
    import { changeSettings, previewSettings } from "$lib/supervision.svelte.js";
    import { applyRefusals, diffPreset, readPreset, textBytes, type Preset, type PresetRow, type Setting } from "$lib/settings.js";
    import StatusBadge from "./StatusBadge.svelte";

    type Props = {
        open: boolean;
        app: string;
        settings: Setting[];
        done: (message: string) => void;
    };

    let { open = $bindable(), app, settings, done }: Props = $props();

    let preset = $state<Preset | null>(null);
    let rows = $state<PresetRow[]>([]);
    let failure = $state("");
    let checking = $state(false);
    let checked = $state(false);
    let busy = $state(false);
    let reason = $state("");
    let attempt = 0;

    $effect(() => {
        if (open) {
            attempt += 1;
            preset = null;
            rows = [];
            failure = "";
            reason = "";
            checking = false;
            checked = false;
        }
    });

    const changes = $derived(rows.filter((row) => row.status === "change"));
    const refused = $derived(rows.filter((row) => row.status === "refused"));
    const same = $derived(rows.filter((row) => row.status === "same"));
    const reasonProblem = $derived(reason.trim() === "" || textBytes(reason.trim()) > 255);
    const ready = $derived(preset !== null && checked && !checking && refused.length === 0 && changes.length > 0 && !reasonProblem);

    function problemsOf(refusal: unknown): { key: string; message: string }[] {
        if (!(refusal instanceof ApiError) || refusal.body === null || typeof refusal.body !== "object") return [];
        const errors = (refusal.body as { errors?: unknown }).errors;
        if (!Array.isArray(errors)) return [];
        return errors
            .filter(
                (error): error is { key: string; message: string } =>
                    typeof error === "object" && error !== null && typeof error.key === "string" && typeof error.message === "string",
            )
            .map((error) => ({ key: error.key, message: error.message }));
    }

    async function choose(event: Event & { currentTarget: HTMLInputElement }) {
        const mine = ++attempt;
        failure = "";
        preset = null;
        rows = [];
        checked = false;
        checking = false;
        const file = event.currentTarget.files?.[0];
        if (!file) return;
        const text = await file.text();
        if (mine !== attempt) return;
        const read = readPreset(text);
        if ("error" in read) {
            failure = read.error;
            return;
        }
        preset = read.preset;
        rows = diffPreset(read.preset, settings);
        await check(mine);
    }

    async function check(mine: number) {
        const changing = rows.filter((row) => row.status === "change");
        if (changing.length === 0) {
            checked = true;
            return;
        }
        failure = "";
        checking = true;
        try {
            await previewSettings(
                app,
                changing.map((row) => ({ key: row.key, value: row.next })),
            );
            if (mine === attempt) checked = true;
        } catch (refusal) {
            if (mine !== attempt) return;
            const problems = problemsOf(refusal);
            if (problems.length > 0) {
                rows = applyRefusals(rows, problems);
                checked = true;
            } else failure = refusal instanceof ApiError ? refusal.message : "The app could not check the preset";
        } finally {
            if (mine === attempt) checking = false;
        }
    }

    function checkAgain() {
        if (preset === null || checking) return;
        void check(++attempt);
    }

    async function apply() {
        if (!ready || busy) return;
        busy = true;
        failure = "";
        try {
            const answer = await changeSettings(
                app,
                changes.map((row) => ({ key: row.key, value: row.next })),
                reason.trim(),
            );
            open = false;
            done(answer.message);
        } catch (refusal) {
            const problems = problemsOf(refusal);
            if (problems.length > 0) rows = applyRefusals(rows, problems);
            failure = refusal instanceof ApiError ? refusal.message : "The preset could not be applied";
        } finally {
            busy = false;
        }
    }
</script>

<Dialog.Root bind:open>
    <Dialog.Content class="max-h-dvh overflow-y-auto sm:max-w-3xl">
        <Dialog.Header>
            <Dialog.Title>Import a settings preset into {app}</Dialog.Title>
            <Dialog.Description>
                Every entry is checked before anything changes, and nothing is applied while any entry is refused. Keys the preset does not
                name stay as they are.
            </Dialog.Description>
        </Dialog.Header>

        <div class="space-y-2">
            <Label for="preset-file">Preset file</Label>
            <input
                id="preset-file"
                type="file"
                accept=".json,application/json"
                class="block w-full text-sm file:mr-3 file:rounded-md file:border file:bg-muted file:px-3 file:py-1.5 file:text-sm"
                onchange={(event) => void choose(event)}
            />
        </div>

        {#if failure}
            <p class="rounded-md border border-destructive/30 bg-destructive/5 p-3 text-sm text-destructive" role="alert">{failure}</p>
        {/if}

        {#if preset}
            {#if preset.app !== "" && preset.app !== app}
                <p class="text-sm text-waiting">This preset was exported from {preset.app}.</p>
            {/if}
            <p class="text-sm text-muted-foreground">
                {changes.length} change{changes.length === 1 ? "" : "s"}, {same.length} already held{refused.length > 0
                    ? `, ${refused.length} refused`
                    : ""}{checking ? ", checking with the app" : ""}.
            </p>
            <Table.Root>
                <Table.Header>
                    <Table.Row>
                        <Table.Head>Setting</Table.Head>
                        <Table.Head>Now</Table.Head>
                        <Table.Head>In the preset</Table.Head>
                        <Table.Head>Result</Table.Head>
                    </Table.Row>
                </Table.Header>
                <Table.Body>
                    {#each rows as row (row.key)}
                        <Table.Row>
                            <Table.Cell class="align-top font-mono text-xs break-all whitespace-normal">{row.key}</Table.Cell>
                            <Table.Cell class="align-top font-mono text-xs break-all whitespace-normal">{row.current ?? "—"}</Table.Cell>
                            <Table.Cell class="align-top font-mono text-xs break-all whitespace-normal">{row.next}</Table.Cell>
                            <Table.Cell class="align-top text-xs whitespace-normal">
                                {#if row.status === "change"}
                                    <StatusBadge tone="waiting">Changes</StatusBadge>
                                {:else if row.status === "same"}
                                    <StatusBadge tone="unknown">Already held</StatusBadge>
                                {:else}
                                    <StatusBadge tone="wrong">Refused</StatusBadge>
                                    <div class="mt-1 text-destructive">{row.problem}</div>
                                {/if}
                            </Table.Cell>
                        </Table.Row>
                    {/each}
                </Table.Body>
            </Table.Root>
            {#if failure && !checked && !checking}
                <Button variant="outline" size="sm" onclick={checkAgain}>Check again</Button>
            {/if}
            {#if refused.length === 0 && changes.length > 0}
                <div class="space-y-2">
                    <Label for="preset-reason">Why</Label>
                    <Textarea id="preset-reason" bind:value={reason} rows={2} placeholder="What this preset is for" />
                </div>
            {/if}
        {/if}

        <Dialog.Footer>
            <Button variant="outline" onclick={() => (open = false)}>Leave it</Button>
            <Button disabled={!ready || busy} onclick={() => void apply()}
                >{busy ? "Applying…" : `Apply ${changes.length} change${changes.length === 1 ? "" : "s"}`}</Button
            >
        </Dialog.Footer>
    </Dialog.Content>
</Dialog.Root>
