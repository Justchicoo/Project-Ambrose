<!-- Project Ambrose by Imjustchico: The database page, live from each app: every database it opens with its state, address without the password, the connections in use and the work queued, the update files it has applied, and the pending ones marked data-only or restart-required with the statement or the file they wait behind, and the button that applies the pending data-only files and reloads the stores that read them. -->
<script lang="ts">
    import * as Card from "$lib/components/ui/card/index.js";
    import * as Dialog from "$lib/components/ui/dialog/index.js";
    import * as Select from "$lib/components/ui/select/index.js";
    import * as Table from "$lib/components/ui/table/index.js";
    import { Button } from "$lib/components/ui/button/index.js";
    import { ApiError } from "$lib/api.svelte.js";
    import { applyData, databasesOf, servedBy, supervised, supervisorServes, updatesOf } from "$lib/supervision.svelte.js";
    import type { DatabaseAnswer, DatabaseUpdatesAnswer, PendingUpdate } from "$lib/schemas.js";
    import PlayIcon from "@lucide/svelte/icons/play";
    import { toast } from "svelte-sonner";
    import PageHeader from "../components/PageHeader.svelte";
    import StatusBadge from "../components/StatusBadge.svelte";

    type Tone = "healthy" | "waiting" | "wrong" | "unknown";

    const tones: Record<string, Tone> = {
        open: "healthy",
        updating: "waiting",
        opening: "waiting",
        waiting: "waiting",
        failed: "wrong",
        closed: "unknown",
        unconfigured: "unknown",
    };

    const choices = $derived(supervisorServes() ? [servedBy(), ...supervised().map((app) => app.name)] : [servedBy()]);
    let chosen = $state("");
    let databases = $state<DatabaseAnswer | null>(null);
    let updates = $state<DatabaseUpdatesAnswer | null>(null);
    let failure = $state("");
    let asking = $state("");
    let applying = $state("");

    const app = $derived(choices.includes(chosen) ? chosen : (choices[0] ?? ""));

    $effect(() => {
        const name = app;
        if (name === "") return;
        const controller = new AbortController();
        const readState = async () => {
            try {
                databases = await databasesOf(name, controller.signal);
                failure = "";
            } catch (problem) {
                if (controller.signal.aborted) return;
                databases = null;
                failure = problem instanceof ApiError ? problem.message : "The databases could not be read";
            }
        };
        void readState();
        void readUpdates(name, controller.signal);
        const timer = setInterval(() => void readState(), 2000);
        return () => {
            clearInterval(timer);
            controller.abort();
        };
    });

    async function readUpdates(name: string, signal?: AbortSignal) {
        try {
            updates = await updatesOf(name, signal);
        } catch (problem) {
            if (signal?.aborted) return;
            updates = null;
            if (failure === "") failure = problem instanceof ApiError ? problem.message : "The update files could not be read";
        }
    }

    function listed(name: string) {
        return updates?.databases.find((database) => database.name === name);
    }

    function appliable(name: string): PendingUpdate[] {
        return (listed(name)?.pending ?? []).filter((update) => !update.restart_required);
    }

    async function apply(name: string) {
        applying = name;
        asking = "";
        try {
            const result = await applyData(app, name);
            const reloaded = result.stores.filter((store) => store.loaded).length;
            if (!result.succeeded)
                toast.error(`${result.failed_at ?? "An update"} failed`, { description: result.failure ?? "The apply did not finish." });
            else if (result.applied.length === 0)
                toast(`Nothing to apply to ${name}`, {
                    description: result.stopped_at
                        ? `${result.stopped_at} changes the schema, so it waits for a restart.`
                        : "It is up to date.",
                });
            else
                toast(`Applied ${result.applied.length} update${result.applied.length === 1 ? "" : "s"} to ${name}`, {
                    description: `${result.applied.join(", ")}${reloaded > 0 ? `; reloaded ${reloaded} store${reloaded === 1 ? "" : "s"}` : ""}`,
                });
            for (const store of result.stores)
                if (!store.loaded) toast.error(`${store.name} did not reload`, { description: store.errors.join("; ") });
            await readUpdates(app);
        } catch (problem) {
            const refused = problem instanceof ApiError ? problem : null;
            toast.error(`The update could not be applied to ${name}`, {
                description: refused
                    ? `${refused.message}${refused.requestId ? ` (request ${refused.requestId})` : ""}`
                    : "The app did not answer",
            });
        } finally {
            applying = "";
        }
    }
</script>

<PageHeader
    title="Database"
    description={`The databases ${app === "" ? "this app" : app} opens, what the updater has applied, and what is still waiting.`}
>
    {#snippet actions()}
        {#if choices.length > 1}
            <Select.Root type="single" bind:value={chosen}>
                <Select.Trigger class="w-44" aria-label="App whose databases are shown">{app}</Select.Trigger>
                <Select.Content>
                    {#each choices as name (name)}
                        <Select.Item value={name}>{name}</Select.Item>
                    {/each}
                </Select.Content>
            </Select.Root>
        {/if}
    {/snippet}
</PageHeader>

{#if failure}
    <Card.Root class="shadow-xs">
        <Card.Header>
            <Card.Title>The databases could not be read</Card.Title>
            <Card.Description>{failure}</Card.Description>
        </Card.Header>
    </Card.Root>
{:else if !databases}
    <p class="text-sm text-muted-foreground">Reading the databases of {app === "" ? "this app" : app}.</p>
{:else if databases.databases.length === 0}
    <p class="text-sm text-muted-foreground">{app} opens no database.</p>
{:else}
    {#each databases.databases as database (database.name)}
        {@const files = listed(database.name)}
        {@const ready = appliable(database.name)}
        <Card.Root class="shadow-xs">
            <Card.Header>
                <Card.Title class="font-serif text-xl">{database.name}</Card.Title>
                <Card.Description class="font-mono text-xs">{database.address ?? "No connection string"}</Card.Description>
                <Card.Action>
                    <div class="flex items-center gap-2">
                        <StatusBadge tone={tones[database.state] ?? "unknown"} pulse={database.state === "open"}
                            >{database.state.charAt(0).toUpperCase() + database.state.slice(1)}</StatusBadge
                        >
                        {#if database.applying}<StatusBadge tone="waiting">Applying</StatusBadge>{/if}
                        {#if !database.updates_enabled}<StatusBadge tone="unknown">Updates off</StatusBadge>{/if}
                    </div>
                </Card.Action>
            </Card.Header>
            <Card.Content class="space-y-4">
                <dl class="grid grid-cols-2 gap-x-4 gap-y-3 text-sm @2xl/main:grid-cols-4">
                    <div>
                        <dt class="text-xs text-muted-foreground">Connections</dt>
                        <dd class="font-medium tabular-nums">
                            {database.pool.async_connections} async, {database.pool.sync_connections} sync
                        </dd>
                    </div>
                    <div>
                        <dt class="text-xs text-muted-foreground">In use now</dt>
                        <dd class="font-medium tabular-nums">
                            {database.pool.async_active} running, {database.pool.sync_leased} leased
                        </dd>
                    </div>
                    <div>
                        <dt class="text-xs text-muted-foreground">Queued work</dt>
                        <dd class="font-medium tabular-nums">{database.pool.queued}</dd>
                    </div>
                    <div>
                        <dt class="text-xs text-muted-foreground">Reconnects</dt>
                        <dd class="font-medium tabular-nums">{database.pool.reconnects}</dd>
                    </div>
                </dl>

                {#if files && !files.listed}
                    <p class="text-sm text-muted-foreground">Its update files could not be read: {files.error}</p>
                {:else if files}
                    <div class="space-y-2">
                        <div class="flex flex-wrap items-center justify-between gap-2">
                            <h2 class="text-sm font-medium">
                                {files.pending.length} pending, {files.applied.length} applied
                            </h2>
                            <Button
                                size="sm"
                                disabled={ready.length === 0 || database.state !== "open" || !database.updates_enabled || applying !== ""}
                                onclick={() => (asking = database.name)}
                            >
                                <PlayIcon />Apply {ready.length} data-only update{ready.length === 1 ? "" : "s"}
                            </Button>
                        </div>
                        {#if files.pending.length > 0}
                            <Table.Root>
                                <Table.Header>
                                    <Table.Row class="hover:bg-transparent">
                                        <Table.Head>Pending file</Table.Head>
                                        <Table.Head>Kind</Table.Head>
                                        <Table.Head class="hidden md:table-cell">Why</Table.Head>
                                    </Table.Row>
                                </Table.Header>
                                <Table.Body>
                                    {#each files.pending as update (update.name)}
                                        <Table.Row>
                                            <Table.Cell class="font-mono text-xs">{update.name}</Table.Cell>
                                            <Table.Cell>
                                                {#if update.kind === "rename"}
                                                    <StatusBadge tone="mine">Renamed</StatusBadge>
                                                {:else if update.restart_required}
                                                    <StatusBadge tone="waiting">Restart required</StatusBadge>
                                                {:else}
                                                    <StatusBadge tone="healthy">Data only</StatusBadge>
                                                {/if}
                                            </Table.Cell>
                                            <Table.Cell class="hidden text-xs text-muted-foreground md:table-cell">
                                                {#if update.problem}
                                                    {update.problem}
                                                {:else if update.waits_for}
                                                    It runs after {update.waits_for}, which changes the schema
                                                {:else if update.statement}
                                                    Line {update.line}: <span class="font-mono">{update.statement}</span>
                                                {:else if update.renamed_from}
                                                    The same file was applied as {update.renamed_from}
                                                {:else}
                                                    It only changes rows{update.transactional ? ", inside one transaction" : ""}
                                                {/if}
                                            </Table.Cell>
                                        </Table.Row>
                                    {/each}
                                </Table.Body>
                            </Table.Root>
                        {:else}
                            <p class="text-sm text-muted-foreground">Every update file has been applied.</p>
                        {/if}
                        {#if files.applied.some((update) => update.changed || !update.present)}
                            <p class="text-xs text-waiting">
                                {files.applied.filter((update) => update.changed).length} applied file{files.applied.filter(
                                    (update) => update.changed,
                                ).length === 1
                                    ? ""
                                    : "s"} changed since they were applied, and
                                {files.applied.filter((update) => !update.present).length} are no longer in any update folder.
                            </p>
                        {/if}
                    </div>
                {/if}
                {#if database.stores.length > 0}
                    <p class="text-xs text-muted-foreground">
                        Stores reading it: {database.stores.join(", ")}. They reload when a data-only update is applied.
                    </p>
                {/if}
            </Card.Content>
        </Card.Root>
    {/each}
{/if}

<Dialog.Root open={asking !== ""} onOpenChange={(open) => (asking = open ? asking : "")}>
    <Dialog.Content>
        <Dialog.Header>
            <Dialog.Title>Apply the pending data-only updates to {asking}?</Dialog.Title>
            <Dialog.Description>
                They are applied in order, each inside one transaction where the file allows it, and the apply stops before the first file
                that can change the schema. The stores reading this database reload afterwards, keeping what they hold if a reload fails.
            </Dialog.Description>
        </Dialog.Header>
        <Dialog.Footer>
            <Button variant="outline" onclick={() => (asking = "")}>Leave it</Button>
            <Button onclick={() => void apply(asking)}>Apply them</Button>
        </Dialog.Footer>
    </Dialog.Content>
</Dialog.Root>
