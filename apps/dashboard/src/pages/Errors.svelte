<!-- Project Ambrose by Imjustchico: Lists persisted errors across the supervised apps and creates a local report only after the operator previews the exact selected contents. -->
<script lang="ts">
    import * as Card from "$lib/components/ui/card/index.js";
    import * as Table from "$lib/components/ui/table/index.js";
    import { Button } from "$lib/components/ui/button/index.js";
    import { session } from "$lib/api.svelte.js";
    import { ApiError } from "$lib/api.svelte.js";
    import { clearErrorGroup, createErrorReport, errors, previewErrorReport } from "$lib/supervision.svelte.js";
    import type { InferOutput } from "valibot";
    import { ErrorsAnswer, ErrorReportAnswer } from "$lib/schemas.js";
    import { onMount } from "svelte";
    import PageHeader from "../components/PageHeader.svelte";

    type ErrorGroup = InferOutput<typeof ErrorsAnswer>["groups"][number];
    type Report = InferOutput<typeof ErrorReportAnswer>["report"];
    const issueForm = "https://github.com/Justchicoo/Project-Ambrose/issues/new";

    let groups = $state<ErrorGroup[]>([]);
    let selected = $state<number[]>([]);
    let includeRendered = $state(false);
    let preview = $state<Report | null>(null);
    let previewKey = $state("");
    let loading = $state(true);
    let busy = $state(false);
    let failure = $state("");
    let notice = $state("");
    let downloaded = $state(false);
    let clearing = $state<number | null>(null);

    const canReport = $derived(session.via === "token" || !session.panel || session.user?.permissions.includes("errors.report") === true);
    const canClear = $derived(session.via === "token" || !session.panel || session.user?.permissions.includes("errors.clear") === true);
    const requestKey = $derived(JSON.stringify({ ids: [...selected].sort((left, right) => left - right), includeRendered }));
    const previewMatches = $derived(preview !== null && previewKey === requestKey && selected.length > 0);
    const reportText = $derived(previewMatches && preview !== null ? JSON.stringify(preview, null, 2) : "");

    function sourceUrl(group: ErrorGroup): string | null {
        if (group.revision === "" || !group.file.startsWith("src/") || group.file.split("/").includes("..")) return null;
        const path = group.file.split("/").map(encodeURIComponent).join("/");
        return `https://github.com/Justchicoo/Project-Ambrose/blob/${encodeURIComponent(group.revision)}/${path}#L${group.line}`;
    }

    function when(epochMs: number): string {
        const date = new Date(epochMs);
        return Number.isNaN(date.getTime()) ? "Unknown time" : date.toLocaleString();
    }

    async function refresh() {
        loading = true;
        failure = "";
        try {
            groups = (await errors()).groups;
            selected = selected.filter((id) => groups.some((group) => group.id === id));
            preview = null;
            previewKey = "";
            downloaded = false;
        } catch (problem) {
            failure = problem instanceof ApiError ? problem.message : "The error groups could not be read";
        } finally {
            loading = false;
        }
    }

    function selectGroup(id: number, checked: boolean) {
        selected = checked ? [...new Set([...selected, id])] : selected.filter((entry) => entry !== id);
        preview = null;
        downloaded = false;
        notice = "";
    }

    async function clearGroup(id: number) {
        clearing = id;
        failure = "";
        notice = "";
        try {
            await clearErrorGroup(id);
            notice = "The error group was cleared and audited.";
            await refresh();
        } catch (problem) {
            failure = problem instanceof ApiError ? problem.message : "The error group could not be cleared";
        } finally {
            clearing = null;
        }
    }

    async function makePreview() {
        if (selected.length === 0) return;
        busy = true;
        failure = "";
        notice = "";
        preview = null;
        downloaded = false;
        const key = requestKey;
        try {
            const answer = await previewErrorReport(selected, includeRendered);
            preview = answer.report;
            previewKey = key;
        } catch (problem) {
            failure = problem instanceof ApiError ? problem.message : "The report preview could not be built";
        } finally {
            busy = false;
        }
    }

    async function createReport() {
        if (!previewMatches || preview === null || !canReport) return;
        busy = true;
        failure = "";
        notice = "";
        try {
            const answer = await createErrorReport(selected, includeRendered);
            const contents = JSON.stringify(answer.report, null, 2);
            if (contents !== reportText) {
                preview = null;
                previewKey = "";
                failure = "The report changed after preview. Review a new preview before creating it.";
                return;
            }
            const file = new Blob([`${contents}\n`], { type: "application/json;charset=utf-8" });
            const url = URL.createObjectURL(file);
            const link = document.createElement("a");
            link.href = url;
            link.download = `ambrose-error-report-${new Date().toISOString().replaceAll(":", "-")}.json`;
            document.body.append(link);
            link.click();
            link.remove();
            window.setTimeout(() => URL.revokeObjectURL(url), 0);
            downloaded = true;
            notice = "The report was audited and downloaded. It has not been sent anywhere.";
        } catch (problem) {
            failure = problem instanceof ApiError ? problem.message : "The report could not be created";
        } finally {
            busy = false;
        }
    }

    onMount(() => {
        void refresh();
    });
</script>

<PageHeader title="Error reports" description="Review source locations and choose exactly what to include before creating a report.">
    {#snippet actions()}
        <Button variant="outline" onclick={() => void refresh()} disabled={loading || busy}>Refresh</Button>
    {/snippet}
</PageHeader>

{#if failure}
    <p class="rounded-md border border-destructive/30 bg-destructive/5 p-3 text-sm text-destructive" role="alert">{failure}</p>
{/if}
{#if notice}
    <p class="rounded-md border border-healthy/30 bg-healthy/5 p-3 text-sm text-healthy" role="status">{notice}</p>
{/if}

<Card.Root>
    <Card.Header>
        <Card.Title>{groups.length} error group(s)</Card.Title>
        <Card.Description>Repeated errors are grouped by app, category, source location and message template.</Card.Description>
    </Card.Header>
    <Card.Content>
        {#if loading}
            <p class="py-6 text-sm text-muted-foreground" aria-busy="true">Reading error groups…</p>
        {:else if groups.length === 0}
            <p class="py-6 text-sm text-muted-foreground">No errors have been reported by the supervised apps.</p>
        {:else}
            <Table.Root>
                <Table.Header>
                    <Table.Row>
                        <Table.Head><span class="sr-only">Select</span></Table.Head>
                        <Table.Head>App and category</Table.Head>
                        <Table.Head>Source</Table.Head>
                        <Table.Head>Count</Table.Head>
                        <Table.Head>First seen</Table.Head>
                        <Table.Head>Last seen</Table.Head>
                        <Table.Head>Template</Table.Head>
                        {#if canClear}<Table.Head><span class="sr-only">Actions</span></Table.Head>{/if}
                    </Table.Row>
                </Table.Header>
                <Table.Body>
                    {#each groups as group (group.id)}
                        <Table.Row>
                            <Table.Cell>
                                <input
                                    type="checkbox"
                                    class="size-4 rounded border-input accent-primary"
                                    checked={selected.includes(group.id)}
                                    disabled={busy || clearing !== null}
                                    aria-label={`Select ${group.app} ${group.category} at ${group.file}:${group.line}`}
                                    onchange={(event) => selectGroup(group.id, event.currentTarget.checked)}
                                />
                            </Table.Cell>
                            <Table.Cell>
                                <div class="font-medium">{group.app}</div>
                                <div class="text-xs text-muted-foreground">{group.category} · {group.level}</div>
                            </Table.Cell>
                            <Table.Cell>
                                {#if sourceUrl(group)}
                                    <a
                                        class="font-mono text-xs underline decoration-muted-foreground underline-offset-4"
                                        href={sourceUrl(group) ?? undefined}
                                        target="_blank"
                                        rel="noreferrer">{group.file}:{group.line}</a
                                    >
                                {:else}
                                    <span class="font-mono text-xs">{group.file}:{group.line}</span>
                                {/if}
                                <div class="text-xs text-muted-foreground">{group.function} · {group.revision}</div>
                            </Table.Cell>
                            <Table.Cell class="tabular-nums">
                                <div>{group.total_count} total</div>
                                <div class="text-xs text-muted-foreground">{group.count} this run</div>
                            </Table.Cell>
                            <Table.Cell class="text-xs">{when(group.first_epoch_ms)}</Table.Cell>
                            <Table.Cell class="text-xs">{when(group.last_epoch_ms)}</Table.Cell>
                            <Table.Cell>
                                <code class="whitespace-pre-wrap text-xs">{group.template}</code>
                                {#if group.context_before.length > 0}
                                    <details class="mt-2 text-xs">
                                        <summary class="cursor-pointer text-muted-foreground">
                                            {group.context_before.length} log line(s) before latest occurrence
                                        </summary>
                                        <ol class="mt-2 space-y-2 border-l pl-3">
                                            {#each group.context_before as record (record.sequence)}
                                                <li>
                                                    <div class="text-muted-foreground">
                                                        {record.time} · {record.level} · {record.category}
                                                    </div>
                                                    <pre class="whitespace-pre-wrap">{record.message}</pre>
                                                </li>
                                            {/each}
                                        </ol>
                                    </details>
                                {/if}
                            </Table.Cell>
                            {#if canClear}
                                <Table.Cell>
                                    <Button
                                        variant="ghost"
                                        size="sm"
                                        disabled={clearing !== null || busy}
                                        onclick={() => void clearGroup(group.id)}
                                        aria-label={`Clear ${group.app} error at ${group.file}:${group.line}`}
                                    >
                                        {clearing === group.id ? "Clearing…" : "Clear"}
                                    </Button>
                                </Table.Cell>
                            {/if}
                        </Table.Row>
                    {/each}
                </Table.Body>
            </Table.Root>
        {/if}
    </Card.Content>
</Card.Root>

<Card.Root>
    <Card.Header>
        <Card.Title>Create a report</Card.Title>
        <Card.Description>The report stays on this machine. Nothing is sent to the maintainers automatically.</Card.Description>
    </Card.Header>
    <Card.Content class="space-y-4">
        <div class="flex items-start gap-3 text-sm">
            <input
                id="include-rendered"
                type="checkbox"
                class="mt-0.5 size-4 rounded border-input accent-primary"
                checked={includeRendered}
                disabled={busy || clearing !== null}
                onchange={(event) => {
                    includeRendered = event.currentTarget.checked;
                    preview = null;
                    previewKey = "";
                    downloaded = false;
                    notice = "";
                }}
            />
            <label for="include-rendered">
                Include rendered messages and the log lines before each occurrence.
                <span class="mt-1 block text-xs text-muted-foreground">
                    Leave this off to include only source, template, counts and times. Included text is redacted, but review it before
                    sharing.
                </span>
            </label>
        </div>
        <div class="flex flex-wrap gap-2">
            <Button variant="outline" onclick={() => void makePreview()} disabled={busy || selected.length === 0}>
                {busy ? "Working…" : "Preview report"}
            </Button>
            <Button onclick={() => void createReport()} disabled={busy || !previewMatches || !canReport}>Create and download</Button>
            {#if session.panel && !canReport}
                <span class="self-center text-xs text-muted-foreground">Your role can preview reports but does not have errors.report.</span
                >
            {/if}
        </div>
        {#if previewMatches}
            <div class="space-y-3">
                <h2 class="font-medium">Exact report contents</h2>
                <pre
                    class="max-h-[32rem] overflow-auto rounded-md border bg-muted/30 p-4 text-xs whitespace-pre-wrap"
                    aria-label="Exact report contents">{reportText}</pre>
                {#if downloaded}
                    <p class="text-sm">
                        Attach the downloaded file to
                        <a class="underline underline-offset-4" href={issueForm} target="_blank" rel="noreferrer">a repository issue</a>.
                    </p>
                {/if}
            </div>
        {:else if preview !== null || selected.length > 0}
            <p class="text-xs text-muted-foreground">Preview the current selection before creating the report.</p>
        {/if}
    </Card.Content>
</Card.Root>
