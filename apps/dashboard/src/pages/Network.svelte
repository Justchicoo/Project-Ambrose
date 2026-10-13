<!-- Project Ambrose by Imjustchico: The network page, per node and per app: every port allocation with its bind address, port, protocol, public alias, the app and role using it and notes, adding one or a range with every port proven free, assigning one to an app through its listen setting, deleting with the assigned ones refused on every path, and the port pool that supplies ports for new realms. -->
<script lang="ts">
    import * as Card from "$lib/components/ui/card/index.js";
    import * as Dialog from "$lib/components/ui/dialog/index.js";
    import * as Select from "$lib/components/ui/select/index.js";
    import * as Table from "$lib/components/ui/table/index.js";
    import * as Tabs from "$lib/components/ui/tabs/index.js";
    import { Button } from "$lib/components/ui/button/index.js";
    import { Input } from "$lib/components/ui/input/index.js";
    import { Label } from "$lib/components/ui/label/index.js";
    import { SvelteSet } from "svelte/reactivity";
    import { ApiError } from "$lib/api.svelte.js";
    import { may } from "$lib/permission.svelte.js";
    import { toast } from "svelte-sonner";
    import PageHeader from "../components/PageHeader.svelte";
    import {
        addAllocation,
        addRange,
        appOf,
        assignAllocation,
        bulkDeleteAllocations,
        deleteAllocation,
        nodeOf,
        setPool,
        takeFromPool,
        unassignAllocation,
        roles,
        apps,
        type Allocation,
        type AppAnswer,
        type NodeAnswer,
        type Pool,
    } from "$lib/network.js";

    const node = "local";
    const canEdit = $derived(may("network.edit"));

    let view = $state<"node" | "app">("node");
    let chosenApp = $state("");
    let nodeData = $state<NodeAnswer | null>(null);
    let appData = $state<AppAnswer | null>(null);
    let failure = $state("");
    let loading = $state(true);

    let selected = new SvelteSet<number>();
    let adding = $state(false);
    let assigning = $state<Allocation | null>(null);
    let poolEditing = $state(false);

    let draftAddress = $state("0.0.0.0");
    let draftPort = $state("");
    let draftEndPort = $state("");
    let draftProtocol = $state("tcp");
    let draftAlias = $state("");
    let draftNotes = $state("");
    let isRange = $state(false);

    let assignApp = $state("gameserver");
    let assignRole = $state("game listener");
    let assignReason = $state("");

    let poolStart = $state("");
    let poolEnd = $state("");
    let takeCount = $state("1");

    const allocations = $derived(view === "node" ? (nodeData?.allocations ?? []) : (appData?.allocations ?? []));
    const pool: Pool | null | undefined = $derived(view === "node" ? nodeData?.pool : undefined);

    async function refresh() {
        loading = true;
        failure = "";
        const controller = new AbortController();
        try {
            if (view === "node") {
                nodeData = await nodeOf(node, controller.signal);
            } else if (chosenApp) {
                appData = await appOf(chosenApp, controller.signal);
            }
        } catch (problem) {
            if (controller.signal.aborted) return;
            failure = problem instanceof ApiError ? problem.message : "The allocations could not be read";
            nodeData = null;
            appData = null;
        } finally {
            loading = false;
            selected.clear();
        }
    }

    $effect(() => {
        void view;
        void chosenApp;
        void refresh();
    });

    function resetDraft() {
        draftAddress = "0.0.0.0";
        draftPort = "";
        draftEndPort = "";
        draftProtocol = "tcp";
        draftAlias = "";
        draftNotes = "";
        isRange = false;
    }

    async function add() {
        const port = Number.parseInt(draftPort, 10);
        const endPort = Number.parseInt(draftEndPort, 10);
        if (!draftAddress || !Number.isInteger(port) || port < 1 || port > 65535) {
            toast.error("A bind address and a port from 1 to 65535 are needed");
            return;
        }
        try {
            if (isRange) {
                if (!Number.isInteger(endPort) || endPort < port || endPort > 65535) {
                    toast.error("The range needs a last port from the first port to 65535");
                    return;
                }
                const result = await addRange({
                    bind_address: draftAddress,
                    port,
                    end_port: endPort,
                    protocol: draftProtocol,
                    public_alias: draftAlias || undefined,
                    notes: draftNotes || undefined,
                });
                toast(`Added ${result.added} allocations`, {
                    description: result.warnings?.join(" "),
                });
            } else {
                const result = await addAllocation({
                    bind_address: draftAddress,
                    port,
                    protocol: draftProtocol,
                    public_alias: draftAlias || undefined,
                    notes: draftNotes || undefined,
                });
                toast(`Allocation ${result.allocation.id} added`, {
                    description: result.allocation.warnings?.join(" "),
                });
            }
            adding = false;
            resetDraft();
            await refresh();
        } catch (problem) {
            const refused = problem instanceof ApiError ? problem : null;
            toast.error("The allocation was not added", {
                description: refused ? refused.message : "The panel did not answer",
            });
        }
    }

    async function assign() {
        if (!assigning) return;
        try {
            const result = await assignAllocation(assigning.id, assignApp, assignRole, assignReason || undefined);
            toast(`Port ${result.port} assigned to ${result.app}`, {
                description: `The app's listen setting was written and it rebinds live.`,
            });
            assigning = null;
            assignReason = "";
            await refresh();
        } catch (problem) {
            const refused = problem instanceof ApiError ? problem : null;
            toast.error("The allocation was not assigned", {
                description: refused ? refused.message : "The panel did not answer",
            });
        }
    }

    async function unassign(allocation: Allocation) {
        try {
            await unassignAllocation(allocation.id);
            toast(`Allocation ${allocation.id} unassigned`);
            await refresh();
        } catch (problem) {
            const refused = problem instanceof ApiError ? problem : null;
            toast.error("The allocation was not unassigned", {
                description: refused ? refused.message : "The panel did not answer",
            });
        }
    }

    async function remove(allocation: Allocation) {
        try {
            await deleteAllocation(allocation.id);
            toast(`Allocation ${allocation.id} deleted`);
            await refresh();
        } catch (problem) {
            const refused = problem instanceof ApiError ? problem : null;
            toast.error("The allocation was not deleted", {
                description: refused ? refused.message : "The panel did not answer",
            });
        }
    }

    async function bulkDelete() {
        const ids = [...selected];
        if (ids.length === 0) return;
        try {
            await bulkDeleteAllocations(ids);
            toast(`Deleted ${ids.length} allocations`);
            await refresh();
        } catch (problem) {
            const refused = problem instanceof ApiError ? problem : null;
            toast.error("Nothing was deleted", {
                description: refused ? refused.message : "The panel did not answer",
            });
        }
    }

    async function savePool() {
        const start = Number.parseInt(poolStart, 10);
        const end = Number.parseInt(poolEnd, 10);
        if (!Number.isInteger(start) || !Number.isInteger(end) || start < 1 || end > 65535 || start > end) {
            toast.error("The pool needs a first and a last port from 1 to 65535");
            return;
        }
        try {
            await setPool(node, start, end);
            toast(`The pool is now ${start}-${end}`);
            poolEditing = false;
            await refresh();
        } catch (problem) {
            const refused = problem instanceof ApiError ? problem : null;
            toast.error("The pool was not changed", {
                description: refused ? refused.message : "The panel did not answer",
            });
        }
    }

    async function take() {
        const count = Number.parseInt(takeCount, 10);
        if (!Number.isInteger(count) || count < 1) {
            toast.error("Take at least one port");
            return;
        }
        try {
            const result = await takeFromPool(node, count);
            toast(`Took ${result.taken.length} ports from the pool`, {
                description: result.taken.map((taken) => taken.port).join(", "),
            });
            await refresh();
        } catch (problem) {
            const refused = problem instanceof ApiError ? problem : null;
            toast.error("No ports were taken", {
                description: refused ? refused.message : "The panel did not answer",
            });
        }
    }

    function toggle(id: number, checked: boolean) {
        if (checked) selected.add(id);
        else selected.delete(id);
    }
</script>

<PageHeader title="Network" description="Every address and port the apps listen on, per node and per app.">
    {#snippet actions()}
        <Tabs.Root bind:value={view}>
            <Tabs.List>
                <Tabs.Trigger value="node">Node</Tabs.Trigger>
                <Tabs.Trigger value="app">App</Tabs.Trigger>
            </Tabs.List>
        </Tabs.Root>
        {#if view === "app"}
            <Select.Root type="single" bind:value={chosenApp}>
                <Select.Trigger class="w-44" aria-label="App whose network is shown">
                    {chosenApp || "Choose an app"}
                </Select.Trigger>
                <Select.Content>
                    {#each apps as name (name)}
                        <Select.Item value={name}>{name}</Select.Item>
                    {/each}
                </Select.Content>
            </Select.Root>
        {/if}
        {#if canEdit}
            <Button onclick={() => (adding = true)}>Add allocation</Button>
        {/if}
    {/snippet}
</PageHeader>

{#if loading}
    <p class="text-sm text-muted-foreground">Reading the allocations.</p>
{:else if failure}
    <Card.Root class="shadow-xs">
        <Card.Header>
            <Card.Title>The allocations could not be read</Card.Title>
            <Card.Description>{failure}</Card.Description>
        </Card.Header>
        <Card.Content>
            <Button variant="outline" onclick={refresh}>Try again</Button>
        </Card.Content>
    </Card.Root>
{:else if view === "app" && !chosenApp}
    <p class="text-sm text-muted-foreground">Choose an app to see its allocations and its live listener.</p>
{:else if allocations.length === 0}
    <Card.Root class="shadow-xs">
        <Card.Content class="py-4 text-sm text-muted-foreground">
            {view === "node" ? "No allocations on this node yet." : `${chosenApp} holds no allocations yet.`}
        </Card.Content>
    </Card.Root>
{:else}
    {#if view === "app" && appData?.live_listener}
        <Card.Root class="mb-4 shadow-xs">
            <Card.Content class="py-4 text-sm">
                <span class="text-muted-foreground">Live listener:</span>
                <span class="font-mono font-medium">{appData.live_listener.bind_address}:{appData.live_listener.port}</span>
            </Card.Content>
        </Card.Root>
    {/if}
    {#if selected.size > 0 && canEdit}
        <div class="mb-4 flex items-center gap-2">
            <span class="text-sm text-muted-foreground">{selected.size} selected</span>
            <Button variant="destructive" size="sm" onclick={bulkDelete}>Delete selected</Button>
        </div>
    {/if}
    <Card.Root class="shadow-xs">
        <Card.Content class="p-0">
            <Table.Root>
                <Table.Header>
                    <Table.Row>
                        {#if canEdit}<Table.Head class="w-10"><span class="sr-only">Select</span></Table.Head>{/if}
                        <Table.Head>Bind address</Table.Head>
                        <Table.Head>Port</Table.Head>
                        <Table.Head>Protocol</Table.Head>
                        <Table.Head>Public alias</Table.Head>
                        <Table.Head>App</Table.Head>
                        <Table.Head>Role</Table.Head>
                        <Table.Head>Notes</Table.Head>
                        {#if canEdit}<Table.Head class="w-40"><span class="sr-only">Actions</span></Table.Head>{/if}
                    </Table.Row>
                </Table.Header>
                <Table.Body>
                    {#each allocations as allocation (allocation.id)}
                        <Table.Row>
                            {#if canEdit}
                                <Table.Cell>
                                    <input
                                        type="checkbox"
                                        class="size-4"
                                        checked={selected.has(allocation.id)}
                                        onchange={(event) => toggle(allocation.id, event.currentTarget.checked)}
                                        aria-label={`Select allocation ${allocation.id}`}
                                    />
                                </Table.Cell>
                            {/if}
                            <Table.Cell class="font-mono">{allocation.bind_address}</Table.Cell>
                            <Table.Cell class="font-mono tabular-nums">{allocation.port}</Table.Cell>
                            <Table.Cell>{allocation.protocol.toUpperCase()}</Table.Cell>
                            <Table.Cell>{allocation.public_alias || "—"}</Table.Cell>
                            <Table.Cell>{allocation.app || "—"}</Table.Cell>
                            <Table.Cell>{allocation.role || "—"}</Table.Cell>
                            <Table.Cell class="max-w-48 truncate">{allocation.notes || "—"}</Table.Cell>
                            {#if canEdit}
                                <Table.Cell>
                                    <div class="flex gap-1">
                                        {#if allocation.assigned}
                                            <Button variant="outline" size="sm" onclick={() => unassign(allocation)}>Unassign</Button>
                                        {:else}
                                            <Button variant="outline" size="sm" onclick={() => (assigning = allocation)}>Assign</Button>
                                        {/if}
                                        <Button variant="ghost" size="sm" onclick={() => remove(allocation)}>Delete</Button>
                                    </div>
                                </Table.Cell>
                            {/if}
                        </Table.Row>
                    {/each}
                </Table.Body>
            </Table.Root>
        </Card.Content>
    </Card.Root>
{/if}

{#if view === "node" && !loading && !failure}
    <Card.Root class="mt-4 shadow-xs">
        <Card.Header>
            <Card.Title>Port pool</Card.Title>
            <Card.Description>
                {pool ? `Ports ${pool.start_port}-${pool.end_port} supply new realms.` : "No pool set for this node."}
            </Card.Description>
            {#if canEdit}
                <Card.Action>
                    <Button
                        variant="outline"
                        size="sm"
                        onclick={() => {
                            poolStart = pool ? String(pool.start_port) : "";
                            poolEnd = pool ? String(pool.end_port) : "";
                            poolEditing = true;
                        }}>Edit pool</Button
                    >
                </Card.Action>
            {/if}
        </Card.Header>
        {#if poolEditing && canEdit}
            <Card.Content class="flex flex-wrap items-end gap-2">
                <div class="grid gap-1">
                    <Label for="pool-start">First port</Label>
                    <Input id="pool-start" inputmode="numeric" bind:value={poolStart} class="w-28" />
                </div>
                <div class="grid gap-1">
                    <Label for="pool-end">Last port</Label>
                    <Input id="pool-end" inputmode="numeric" bind:value={poolEnd} class="w-28" />
                </div>
                <Button size="sm" onclick={savePool}>Save</Button>
                <Button size="sm" variant="ghost" onclick={() => (poolEditing = false)}>Cancel</Button>
            </Card.Content>
        {:else if canEdit}
            <Card.Content class="flex flex-wrap items-end gap-2">
                <div class="grid gap-1">
                    <Label for="take-count">Ports for a new realm</Label>
                    <Input id="take-count" inputmode="numeric" bind:value={takeCount} class="w-28" />
                </div>
                <Button size="sm" variant="outline" onclick={take}>Take from pool</Button>
            </Card.Content>
        {/if}
    </Card.Root>
{/if}

<Dialog.Root bind:open={adding}>
    <Dialog.Content>
        <Dialog.Header>
            <Dialog.Title>Add allocation</Dialog.Title>
            <Dialog.Description>Every port is proven free by binding it before anything is stored.</Dialog.Description>
        </Dialog.Header>
        <div class="grid gap-3">
            <div class="grid gap-1">
                <Label for="new-address">Bind address</Label>
                <Input id="new-address" bind:value={draftAddress} class="font-mono" />
            </div>
            <div class="flex gap-2">
                <div class="grid flex-1 gap-1">
                    <Label for="new-port">{isRange ? "First port" : "Port"}</Label>
                    <Input id="new-port" inputmode="numeric" bind:value={draftPort} />
                </div>
                {#if isRange}
                    <div class="grid flex-1 gap-1">
                        <Label for="new-end-port">Last port</Label>
                        <Input id="new-end-port" inputmode="numeric" bind:value={draftEndPort} />
                    </div>
                {/if}
            </div>
            <div class="grid gap-1">
                <Label for="new-protocol">Protocol</Label>
                <Select.Root type="single" bind:value={draftProtocol}>
                    <Select.Trigger id="new-protocol"><span>{draftProtocol.toUpperCase()}</span></Select.Trigger>
                    <Select.Content>
                        <Select.Item value="tcp">TCP</Select.Item>
                        <Select.Item value="udp">UDP</Select.Item>
                        <Select.Item value="http">HTTP</Select.Item>
                    </Select.Content>
                </Select.Root>
            </div>
            <div class="grid gap-1">
                <Label for="new-alias">Public alias</Label>
                <Input id="new-alias" bind:value={draftAlias} placeholder="players.example.com" />
            </div>
            <div class="grid gap-1">
                <Label for="new-notes">Notes</Label>
                <Input id="new-notes" bind:value={draftNotes} />
            </div>
            <label class="flex items-center gap-2 text-sm">
                <input type="checkbox" class="size-4" checked={isRange} onchange={(event) => (isRange = event.currentTarget.checked)} />
                Add a range of ports (at most 1000)
            </label>
        </div>
        <Dialog.Footer>
            <Button
                variant="ghost"
                onclick={() => {
                    adding = false;
                    resetDraft();
                }}>Cancel</Button
            >
            <Button onclick={add}>Add</Button>
        </Dialog.Footer>
    </Dialog.Content>
</Dialog.Root>

<Dialog.Root
    open={assigning !== null}
    onOpenChange={(open) => {
        if (!open) assigning = null;
    }}
>
    <Dialog.Content>
        <Dialog.Header>
            <Dialog.Title>Assign allocation</Dialog.Title>
            <Dialog.Description>
                {assigning
                    ? `Port ${assigning.port} on ${assigning.bind_address} moves an app's listener live through its settings API.`
                    : ""}
            </Dialog.Description>
        </Dialog.Header>
        <div class="grid gap-3">
            <div class="grid gap-1">
                <Label for="assign-app">App</Label>
                <Select.Root type="single" bind:value={assignApp}>
                    <Select.Trigger id="assign-app"><span>{assignApp}</span></Select.Trigger>
                    <Select.Content>
                        {#each apps as name (name)}
                            <Select.Item value={name}>{name}</Select.Item>
                        {/each}
                    </Select.Content>
                </Select.Root>
            </div>
            <div class="grid gap-1">
                <Label for="assign-role">Role</Label>
                <Select.Root type="single" bind:value={assignRole}>
                    <Select.Trigger id="assign-role"><span>{assignRole}</span></Select.Trigger>
                    <Select.Content>
                        {#each roles as name (name)}
                            <Select.Item value={name}>{name}</Select.Item>
                        {/each}
                    </Select.Content>
                </Select.Root>
            </div>
            <div class="grid gap-1">
                <Label for="assign-reason">Reason</Label>
                <Input id="assign-reason" bind:value={assignReason} placeholder="Why the listener moves" />
            </div>
        </div>
        <Dialog.Footer>
            <Button variant="ghost" onclick={() => (assigning = null)}>Cancel</Button>
            <Button onclick={assign}>Assign</Button>
        </Dialog.Footer>
    </Dialog.Content>
</Dialog.Root>
