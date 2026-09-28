<!-- Project Ambrose by Imjustchico: The files page, over the supervisor's file roots: a root picker and an app picker that narrows the roots to those holding the app's configuration and logs and filters them to its program, the root's policy in words with its volume's free space, breadcrumbs from the root, a filter, sort and refresh, and a table paged by the server with rows picked one by one, a page at a time or as every match of the filter counted from the server's total, each row with an actions button and the same actions on a right click; every control that changes a file is drawn whether or not it can be used, and one that cannot is disabled with its reason beside it, the root's policy first, then a missing permission, then the milestone that brings it; a file opens in a viewer that shows text, says when a file is binary, masks a configuration file's secrets unless the caller may see them and asks, and reads on in windows; an owner edits the root's protected paths; at phone width row and mass actions open in a bottom sheet; and a server that serves no file roots says so. The folder being shown lives in the address, so a link opens it. -->
<script lang="ts">
    import * as Breadcrumb from "$lib/components/ui/breadcrumb/index.js";
    import * as Card from "$lib/components/ui/card/index.js";
    import * as ContextMenu from "$lib/components/ui/context-menu/index.js";
    import * as Dialog from "$lib/components/ui/dialog/index.js";
    import * as DropdownMenu from "$lib/components/ui/dropdown-menu/index.js";
    import * as Select from "$lib/components/ui/select/index.js";
    import * as Sheet from "$lib/components/ui/sheet/index.js";
    import * as Table from "$lib/components/ui/table/index.js";
    import { Button } from "$lib/components/ui/button/index.js";
    import { Input } from "$lib/components/ui/input/index.js";
    import { Label } from "$lib/components/ui/label/index.js";
    import { Textarea } from "$lib/components/ui/textarea/index.js";
    import { ApiError } from "$lib/api.svelte.js";
    import { formatBytes } from "$lib/format.js";
    import { IsMobile } from "$lib/hooks/is-mobile.svelte.js";
    import { may } from "$lib/permission.svelte.js";
    import { fileRoots, fileRules, listFiles, readFile, setFileRules } from "$lib/supervision.svelte.js";
    import type { FileContent, FileEntry, FileListing, FileRootsAnswer } from "$lib/schemas.js";
    import {
        MostCopied,
        breadcrumbs,
        childPath,
        controlState,
        describeSelection,
        folderControls,
        isSelected,
        massControls,
        nothing,
        offersEveryMatch,
        pageHash,
        pageState,
        readPageHash,
        rowControls,
        rowPolicy,
        selectMatching,
        selectPage,
        selectionCount,
        toggleRow,
        viewState,
        type ControlState,
        type PolicyLike,
        type Selection,
    } from "$lib/files.js";
    import ArrowDownIcon from "@lucide/svelte/icons/arrow-down";
    import ArrowUpIcon from "@lucide/svelte/icons/arrow-up";
    import EllipsisIcon from "@lucide/svelte/icons/ellipsis";
    import FileIcon from "@lucide/svelte/icons/file";
    import FolderIcon from "@lucide/svelte/icons/folder";
    import LinkIcon from "@lucide/svelte/icons/link";
    import RefreshCwIcon from "@lucide/svelte/icons/refresh-cw";
    import SearchIcon from "@lucide/svelte/icons/search";
    import ShieldIcon from "@lucide/svelte/icons/shield";
    import XIcon from "@lucide/svelte/icons/x";
    import { onMount } from "svelte";
    import PageHeader from "../components/PageHeader.svelte";
    import StatusBadge from "../components/StatusBadge.svelte";

    type Action = { id: string; label: string; state: ControlState; run?: () => void };
    type SheetFor = { kind: "row"; entry: FileEntry } | { kind: "mass" };

    const limit = 100;
    const mobile = new IsMobile();
    const sorts = [
        { value: "name", label: "Name" },
        { value: "size", label: "Size" },
        { value: "modified", label: "Modified" },
        { value: "kind", label: "Kind" },
    ];

    const start = readPageHash(window.location.hash);
    let roots = $state<FileRootsAnswer | null>(null);
    let rootsFailure = $state("");
    let unavailable = $state(false);
    let rootId = $state(start.root);
    let folder = $state(start.path);
    let appName = $state("");
    let filter = $state("");
    let sort = $state("name");
    let order = $state<"asc" | "desc">("asc");
    let offset = $state(0);
    let refreshed = $state(0);
    let listing = $state<FileListing | null>(null);
    let listingFailure = $state<{ code: string; message: string } | null>(null);
    let selection = $state<Selection>(nothing);
    let notice = $state("");
    let viewing = $state<FileContent | null>(null);
    let viewingPath = $state("");
    let viewFailure = $state("");
    let viewBusy = $state(false);
    let sheetOpen = $state(false);
    let sheetFor = $state<SheetFor | null>(null);
    let rulesOpen = $state(false);
    let rulesText = $state("");
    let rulesReason = $state("");
    let rulesFailure = $state("");
    let rulesBusy = $state(false);
    let builtIn = $state<{ pattern: string; effect: string; why: string }[]>([]);

    const shownRoots = $derived((roots?.roots ?? []).filter((root) => appName === "" || root.apps.includes(appName)));
    const root = $derived((roots?.roots ?? []).find((candidate) => candidate.id === rootId) ?? null);
    const folderPolicy = $derived<PolicyLike | null>(listing?.policy ?? root?.policy ?? null);
    const entries = $derived(listing?.entries ?? []);
    const visible = $derived(entries.map((entry) => entry.name));
    const total = $derived(listing?.total ?? 0);
    const picked = $derived(selectionCount(selection, total));
    const crumbs = $derived(breadcrumbs(rootId, listing?.path ?? folder));
    const everyOnPage = $derived(pageState(selection, visible));

    onMount(() => {
        const controller = new AbortController();
        void (async () => {
            try {
                const answer = await fileRoots(controller.signal);
                roots = answer;
                rootsFailure = "";
                if (!answer.roots.some((candidate) => candidate.id === rootId)) {
                    rootId = (answer.roots.find((candidate) => candidate.present) ?? answer.roots[0])?.id ?? "";
                    folder = "";
                }
            } catch (problem) {
                if (controller.signal.aborted) return;
                if (problem instanceof ApiError && problem.status === 404) unavailable = true;
                else rootsFailure = problem instanceof ApiError ? problem.message : "The file roots could not be read";
            }
        })();
        const follow = () => {
            if (!window.location.hash.startsWith("#files")) return;
            const place = readPageHash(window.location.hash);
            if (place.root !== "" && (place.root !== rootId || place.path !== folder)) {
                rootId = place.root;
                folder = place.path;
                offset = 0;
                selection = nothing;
            }
        };
        window.addEventListener("hashchange", follow);
        return () => {
            controller.abort();
            window.removeEventListener("hashchange", follow);
        };
    });

    $effect(() => {
        const id = rootId;
        const path = folder;
        const asked = { q: filter, sort, order, offset, limit };
        void refreshed;
        if (roots === null || id === "") return;
        const controller = new AbortController();
        void (async () => {
            try {
                const answer = await listFiles(id, { path, ...asked }, controller.signal);
                listing = answer;
                listingFailure = null;
            } catch (problem) {
                if (controller.signal.aborted) return;
                listing = null;
                listingFailure =
                    problem instanceof ApiError
                        ? { code: problem.code, message: problem.message }
                        : { code: "unreachable", message: "This folder could not be read" };
            }
        })();
        return () => controller.abort();
    });

    function hiddenLine(keys: string[]): string {
        return `${keys.length} secret value${keys.length === 1 ? " is" : "s are"} hidden: ${keys.join(", ")}.`;
    }

    function remember() {
        try {
            history.replaceState(history.state, "", pageHash(rootId, folder));
        } catch {
            return;
        }
    }

    function go(path: string) {
        folder = path;
        offset = 0;
        selection = nothing;
        notice = "";
        remember();
    }

    function chooseRoot(id: string) {
        rootId = id;
        closeViewer();
        go("");
    }

    function chooseApp(name: string) {
        appName = name;
        const program = roots?.apps.find((app) => app.name === name)?.program ?? name;
        filter = name === "" ? "" : program;
        const holding = (roots?.roots ?? []).filter((candidate) => name === "" || candidate.apps.includes(name));
        if (holding.length > 0 && !holding.some((candidate) => candidate.id === rootId)) chooseRoot(holding[0].id);
        else go(folder);
    }

    function refilter(text: string) {
        filter = text;
        offset = 0;
        selection = nothing;
    }

    function open(entry: FileEntry) {
        if (entry.kind === "folder") {
            if (entry.openable && canList(entry)) go(childPath(listing?.path ?? folder, entry.name));
            return;
        }
        if (entry.kind === "file" && entry.openable) void view(childPath(listing?.path ?? folder, entry.name));
    }

    function canList(entry: FileEntry): boolean {
        return listing === null || rowPolicy(listing.policy, entry.rule).operations.list?.allowed !== false;
    }

    async function view(path: string, reveal = false) {
        viewBusy = true;
        viewingPath = path;
        viewFailure = "";
        try {
            viewing = await readFile(rootId, path, { reveal });
        } catch (problem) {
            viewing = null;
            viewFailure = problem instanceof ApiError ? problem.message : "The file could not be read";
        } finally {
            viewBusy = false;
        }
    }

    async function readOn() {
        const current = viewing;
        if (current === null || current.next_offset === null) return;
        viewBusy = true;
        try {
            const next = await readFile(rootId, viewingPath, { offset: current.next_offset });
            viewing = {
                ...next,
                offset: current.offset,
                text: (current.text ?? "") + (next.text ?? ""),
                length: current.length + next.length,
            };
        } catch (problem) {
            viewFailure = problem instanceof ApiError ? problem.message : "The rest of the file could not be read";
        } finally {
            viewBusy = false;
        }
    }

    function closeViewer() {
        viewing = null;
        viewingPath = "";
        viewFailure = "";
    }

    function pathOf(name: string): string {
        return `${rootId}:${childPath(listing?.path ?? folder, name)}`;
    }

    async function copyText(text: string, what: string) {
        try {
            await navigator.clipboard.writeText(text);
            notice = `Copied ${what}`;
        } catch {
            notice = `The browser would not copy ${what}; select it by hand instead`;
        }
    }

    async function copySelected() {
        if (selection.kind === "rows") {
            await copyText(
                selection.names.map(pathOf).join("\n"),
                `${selection.names.length} path${selection.names.length === 1 ? "" : "s"}`,
            );
            return;
        }
        if (selection.kind !== "matching") return;
        try {
            const every = await listFiles(rootId, { path: listing?.path ?? folder, q: filter, sort, order, offset: 0, limit: MostCopied });
            await copyText(every.entries.map((entry) => pathOf(entry.name)).join("\n"), `${every.entries.length} paths`);
        } catch (problem) {
            notice = problem instanceof ApiError ? problem.message : "The matching paths could not be read";
        }
    }

    const copyState = $derived<ControlState>(
        selection.kind === "matching" && total > MostCopied
            ? { enabled: false, because: "policy", reason: `Copy paths reaches at most ${MostCopied} matches at once; narrow the filter` }
            : { enabled: true, because: null, reason: "" },
    );

    function rowActions(entry: FileEntry): Action[] {
        const policy = listing ? rowPolicy(listing.policy, entry.rule) : null;
        const opening: ControlState = !entry.openable
            ? { enabled: false, because: "policy", reason: entry.problem ?? "This entry cannot be opened here" }
            : entry.kind === "folder"
              ? canList(entry)
                  ? { enabled: true, because: null, reason: "" }
                  : { enabled: false, because: "policy", reason: entry.rule?.why ?? "This folder is not listed here" }
              : viewState(policy, may);
        return [
            { id: "open", label: entry.kind === "folder" ? "Open" : "View", state: opening, run: () => open(entry) },
            {
                id: "copy-path",
                label: "Copy path",
                state: { enabled: true, because: null, reason: "" },
                run: () => void copyText(pathOf(entry.name), "the path"),
            },
            ...rowControls.map((control) => ({ id: control.id, label: control.label, state: controlState(control, policy, may) })),
        ];
    }

    const massActions = $derived<Action[]>([
        { id: "copy-paths", label: "Copy paths", state: copyState, run: () => void copySelected() },
        ...massControls.map((control) => ({ id: control.id, label: control.label, state: controlState(control, folderPolicy, may) })),
    ]);

    function openSheet(target: SheetFor) {
        sheetFor = target;
        sheetOpen = true;
    }

    function act(action: Action) {
        if (!action.state.enabled || !action.run) return;
        sheetOpen = false;
        action.run();
    }

    function partly(on: boolean): (node: HTMLInputElement) => void {
        return (node) => {
            node.indeterminate = on;
        };
    }

    async function openRules() {
        rulesFailure = "";
        rulesReason = "";
        rulesOpen = true;
        try {
            const answer = await fileRules(rootId);
            rulesText = answer.patterns.join("\n");
            builtIn = answer.built_in;
        } catch (problem) {
            rulesFailure = problem instanceof ApiError ? problem.message : "The protected paths could not be read";
        }
    }

    async function saveRules() {
        rulesBusy = true;
        rulesFailure = "";
        try {
            const answer = await setFileRules(rootId, rulesText.split("\n"), rulesReason.trim());
            rulesOpen = false;
            notice = answer.rebuilt
                ? `The protected paths of ${rootId} are saved and in force`
                : `The protected paths are saved, but the roots were not rebuilt: ${answer.errors.join("; ")}`;
            refreshed += 1;
        } catch (problem) {
            rulesFailure =
                problem instanceof ApiError ? (problem.fields.patterns ?? problem.message) : "The protected paths could not be saved";
        } finally {
            rulesBusy = false;
        }
    }

    function when(epochMs: number): string {
        return epochMs > 0 ? new Date(epochMs).toLocaleString() : "—";
    }

    function kindOf(entry: FileEntry): string {
        if (entry.kind === "folder") return "Folder";
        if (entry.kind === "file") {
            const dot = entry.name.lastIndexOf(".");
            return dot > 0 ? `${entry.name.slice(dot + 1).toUpperCase()} file` : "File";
        }
        return entry.kind === "link" ? "Link" : entry.kind.charAt(0).toUpperCase() + entry.kind.slice(1);
    }

    function effectWord(effect: string): string {
        if (effect === "client_derived") return "Client-derived";
        if (effect === "read_only") return "Read-only";
        if (effect === "elsewhere") return "Another root";
        return "Protected";
    }
</script>

{#snippet actionList(list: Action[], prefix: string)}
    <div class="flex flex-col gap-1">
        {#each list as action (action.id)}
            <Button
                variant="ghost"
                class="h-auto justify-start py-2 text-left"
                disabled={!action.state.enabled}
                aria-describedby={action.state.enabled ? undefined : `${prefix}-${action.id}-why`}
                onclick={() => act(action)}
            >
                <span class="flex flex-col items-start gap-0.5">
                    <span>{action.label}</span>
                    {#if !action.state.enabled}
                        <span id={`${prefix}-${action.id}-why`} class="text-xs font-normal whitespace-normal text-muted-foreground"
                            >{action.state.reason}</span
                        >
                    {/if}
                </span>
            </Button>
        {/each}
    </div>
{/snippet}

<PageHeader title="Files" description="The server's own files, root by root, behind a jail no request can leave.">
    {#snippet actions()}
        {#if roots !== null && roots.roots.length > 0}
            <Select.Root
                type="single"
                value={appName === "" ? "all" : appName}
                onValueChange={(value) => chooseApp(value === "all" ? "" : value)}
            >
                <Select.Trigger class="w-44" aria-label="App whose files are shown">{appName === "" ? "Every app" : appName}</Select.Trigger
                >
                <Select.Content>
                    <Select.Item value="all">Every app</Select.Item>
                    {#each roots.apps as app (app.name)}
                        <Select.Item value={app.name}>{app.name}</Select.Item>
                    {/each}
                </Select.Content>
            </Select.Root>
            <Select.Root type="single" value={rootId} onValueChange={(value) => chooseRoot(value)}>
                <Select.Trigger class="w-52" aria-label="File root">{root?.label ?? "Choose a root"}</Select.Trigger>
                <Select.Content>
                    {#each shownRoots as candidate (candidate.id)}
                        <Select.Item value={candidate.id}>{candidate.label}{candidate.present ? "" : " (missing)"}</Select.Item>
                    {/each}
                </Select.Content>
            </Select.Root>
        {/if}
        {#if may("files.roots") && root !== null}
            <Button variant="outline" onclick={() => void openRules()}><ShieldIcon />Protected paths</Button>
        {/if}
    {/snippet}
</PageHeader>

{#if unavailable}
    <Card.Root>
        <Card.Header>
            <Card.Title>No file roots here</Card.Title>
            <Card.Description>
                The file roots are served by the supervisor, and the server that served this page is not one. Open the panel from the
                supervisor's own address to browse the server's files.
            </Card.Description>
        </Card.Header>
    </Card.Root>
{:else if rootsFailure !== ""}
    <Card.Root>
        <Card.Content class="py-4 text-sm text-destructive" role="alert">{rootsFailure}</Card.Content>
    </Card.Root>
{:else if roots === null}
    <p class="py-6 text-sm text-muted-foreground" aria-busy="true">Reading the file roots.</p>
{:else if root === null}
    <p class="py-6 text-sm text-muted-foreground">This supervisor offers no file root.</p>
{:else}
    <section class="space-y-4" aria-label={`The ${root.label} root`}>
        <div class="flex flex-wrap items-center gap-2 text-sm">
            {#if root.client_derived || listing?.policy.client_derived}
                <StatusBadge tone="mine">Client-derived</StatusBadge>
            {/if}
            {#if listing?.policy.read_only ?? root.read_only}
                <StatusBadge tone="unknown">Read-only</StatusBadge>
            {:else}
                <StatusBadge tone="healthy">Writable</StatusBadge>
            {/if}
            <span id="root-policy" class="text-muted-foreground">{folderPolicy?.summary ?? root.policy.summary}</span>
            {#if root.volume}
                <span class="text-muted-foreground" data-testid="volume">
                    {root.volume.name}: {formatBytes(root.volume.free)} free of {formatBytes(root.volume.total)}, keeping {formatBytes(
                        root.volume.minimum,
                    )} free{root.volume.reserved > 0 ? `, ${formatBytes(root.volume.reserved)} held for writes` : ""}
                </span>
            {/if}
        </div>
        {#if !root.present}
            <p class="rounded-md border p-3 text-sm text-muted-foreground" role="status">
                {root.problem ?? "This root's folder is missing."}
            </p>
        {/if}
        {#if notice !== ""}
            <p class="rounded-md border border-healthy/30 bg-healthy/5 p-3 text-sm" role="status">{notice}</p>
        {/if}

        <Breadcrumb.Root>
            <Breadcrumb.List>
                {#each crumbs as crumb, index (crumb.path)}
                    {#if index > 0}<Breadcrumb.Separator />{/if}
                    <Breadcrumb.Item>
                        {#if index === crumbs.length - 1}
                            <Breadcrumb.Page>{crumb.name}</Breadcrumb.Page>
                        {:else}
                            <Breadcrumb.Link
                                href={pageHash(rootId, crumb.path)}
                                onclick={(event) => {
                                    event.preventDefault();
                                    go(crumb.path);
                                }}>{crumb.name}</Breadcrumb.Link
                            >
                        {/if}
                    </Breadcrumb.Item>
                {/each}
            </Breadcrumb.List>
        </Breadcrumb.Root>

        <div class="flex flex-wrap items-center gap-2">
            <div class="relative">
                <SearchIcon class="absolute top-2.5 left-2.5 size-4 text-muted-foreground" />
                <Input
                    class="w-56 pl-8"
                    placeholder="Filter by name"
                    aria-label="Filter by name"
                    value={filter}
                    oninput={(event) => refilter(event.currentTarget.value)}
                />
            </div>
            <Select.Root
                type="single"
                value={sort}
                onValueChange={(value) => {
                    sort = value;
                    offset = 0;
                }}
            >
                <Select.Trigger class="w-36" aria-label="Sort by">{sorts.find((one) => one.value === sort)?.label ?? "Name"}</Select.Trigger
                >
                <Select.Content>
                    {#each sorts as one (one.value)}
                        <Select.Item value={one.value}>{one.label}</Select.Item>
                    {/each}
                </Select.Content>
            </Select.Root>
            <Button
                variant="outline"
                size="sm"
                aria-label={order === "asc" ? "Sorted ascending; sort descending" : "Sorted descending; sort ascending"}
                onclick={() => {
                    order = order === "asc" ? "desc" : "asc";
                    offset = 0;
                }}
            >
                {#if order === "asc"}<ArrowUpIcon />{:else}<ArrowDownIcon />{/if}
            </Button>
            <Button variant="outline" size="sm" onclick={() => (refreshed += 1)}><RefreshCwIcon />Refresh</Button>
        </div>

        <div class="flex flex-wrap gap-4" role="group" aria-label="Changes to this folder">
            {#each folderControls as control (control.id)}
                {@const state = controlState(control, folderPolicy, may)}
                <div class="flex max-w-60 flex-col gap-1">
                    <Button
                        variant="outline"
                        size="sm"
                        class="self-start"
                        disabled={!state.enabled}
                        aria-describedby={state.enabled ? undefined : `why-${control.id}`}>{control.label}</Button
                    >
                    {#if !state.enabled}
                        <p id={`why-${control.id}`} class="text-xs text-muted-foreground">{state.reason}</p>
                    {/if}
                </div>
            {/each}
        </div>

        {#if picked > 0}
            <div class="flex flex-wrap items-center gap-3 rounded-lg border bg-muted/40 p-3" role="region" aria-label="Selected files">
                <span class="text-sm font-medium" data-testid="selection-count">{describeSelection(selection, total)}</span>
                {#if mobile.current}
                    <Button size="sm" variant="outline" onclick={() => openSheet({ kind: "mass" })}>Actions</Button>
                {:else}
                    {#each massActions as action (action.id)}
                        <div class="flex max-w-52 flex-col gap-1">
                            <Button
                                size="sm"
                                variant="outline"
                                class="self-start"
                                disabled={!action.state.enabled}
                                aria-describedby={action.state.enabled ? undefined : `mass-${action.id}-why`}
                                onclick={() => act(action)}>{action.label}</Button
                            >
                            {#if !action.state.enabled}
                                <p id={`mass-${action.id}-why`} class="text-xs text-muted-foreground">{action.state.reason}</p>
                            {/if}
                        </div>
                    {/each}
                {/if}
                <Button size="sm" variant="ghost" onclick={() => (selection = nothing)}><XIcon />Clear</Button>
            </div>
        {/if}

        {#if listingFailure !== null}
            <Card.Root>
                <Card.Content class="py-4 text-sm text-destructive" role="alert">{listingFailure.message}</Card.Content>
            </Card.Root>
        {:else if listing === null}
            <p class="py-6 text-sm text-muted-foreground" aria-busy="true">Reading {crumbs[crumbs.length - 1]?.name ?? "the folder"}.</p>
        {:else}
            {#if offersEveryMatch(selection, visible, total)}
                <div class="flex flex-wrap items-center gap-2 text-sm">
                    <span>All {visible.length} on this page are selected.</span>
                    <Button size="sm" variant="link" onclick={() => (selection = selectMatching(filter))}
                        >Select all {total} matching</Button
                    >
                </div>
            {/if}
            <Table.Root>
                <Table.Header>
                    <Table.Row>
                        <Table.Head class="w-10">
                            <input
                                type="checkbox"
                                class="size-4"
                                aria-label="Select all on this page"
                                checked={everyOnPage === "all"}
                                disabled={visible.length === 0}
                                {@attach partly(everyOnPage === "some")}
                                onchange={(event) => (selection = selectPage(selection, visible, event.currentTarget.checked))}
                            />
                        </Table.Head>
                        <Table.Head>Name</Table.Head>
                        <Table.Head class="hidden sm:table-cell">Kind</Table.Head>
                        <Table.Head class="text-right">Size</Table.Head>
                        <Table.Head class="hidden md:table-cell">Modified</Table.Head>
                        <Table.Head class="w-12"><span class="sr-only">Actions</span></Table.Head>
                    </Table.Row>
                </Table.Header>
                <Table.Body>
                    {#if entries.length === 0}
                        <Table.Row>
                            <Table.Cell colspan={6} class="py-6 text-center text-sm text-muted-foreground">
                                {filter === "" ? "This folder is empty." : `Nothing here matches "${filter}".`}
                            </Table.Cell>
                        </Table.Row>
                    {/if}
                    {#each entries as entry, index (`${index}/${entry.name}`)}
                        {@const items = rowActions(entry)}
                        <ContextMenu.Root>
                            <ContextMenu.Trigger>
                                {#snippet child({ props })}
                                    <Table.Row {...props} data-state={isSelected(selection, entry.name) ? "selected" : undefined}>
                                        <Table.Cell>
                                            <input
                                                type="checkbox"
                                                class="size-4"
                                                aria-label={`Select ${entry.name}`}
                                                checked={isSelected(selection, entry.name)}
                                                onchange={() => (selection = toggleRow(selection, entry.name, visible))}
                                            />
                                        </Table.Cell>
                                        <Table.Cell class="max-w-72 whitespace-normal">
                                            <div class="flex flex-wrap items-center gap-2">
                                                {#if entry.kind === "folder"}<FolderIcon class="size-4 shrink-0 text-muted-foreground" />
                                                {:else if entry.kind === "link"}<LinkIcon class="size-4 shrink-0 text-muted-foreground" />
                                                {:else}<FileIcon class="size-4 shrink-0 text-muted-foreground" />{/if}
                                                {#if items[0].state.enabled}
                                                    <button
                                                        type="button"
                                                        class="text-left font-medium break-all hover:underline"
                                                        onclick={() => open(entry)}>{entry.name}</button
                                                    >
                                                {:else}
                                                    <span class="font-medium break-all">{entry.name}</span>
                                                {/if}
                                                {#if entry.rule}
                                                    <StatusBadge tone={entry.rule.effect === "client_derived" ? "mine" : "unknown"}
                                                        >{effectWord(entry.rule.effect)}</StatusBadge
                                                    >
                                                {/if}
                                            </div>
                                            {#if entry.problem}
                                                <p class="text-xs text-muted-foreground">{entry.problem}</p>
                                            {/if}
                                        </Table.Cell>
                                        <Table.Cell class="hidden text-muted-foreground sm:table-cell">{kindOf(entry)}</Table.Cell>
                                        <Table.Cell class="text-right tabular-nums"
                                            >{entry.kind === "folder" ? "—" : formatBytes(entry.size)}</Table.Cell
                                        >
                                        <Table.Cell class="hidden text-muted-foreground md:table-cell">{when(entry.modified_ms)}</Table.Cell
                                        >
                                        <Table.Cell class="text-right">
                                            {#if mobile.current}
                                                <Button
                                                    variant="ghost"
                                                    size="icon-sm"
                                                    aria-label={`Actions for ${entry.name}`}
                                                    onclick={() => openSheet({ kind: "row", entry })}><EllipsisIcon /></Button
                                                >
                                            {:else}
                                                <DropdownMenu.Root>
                                                    <DropdownMenu.Trigger>
                                                        {#snippet child({ props: menu })}
                                                            <Button
                                                                variant="ghost"
                                                                size="icon-sm"
                                                                aria-label={`Actions for ${entry.name}`}
                                                                {...menu}><EllipsisIcon /></Button
                                                            >
                                                        {/snippet}
                                                    </DropdownMenu.Trigger>
                                                    <DropdownMenu.Content align="end" class="w-64">
                                                        {#each items as action (action.id)}
                                                            <DropdownMenu.Item
                                                                disabled={!action.state.enabled}
                                                                onSelect={() => act(action)}
                                                            >
                                                                <span class="flex flex-col">
                                                                    <span>{action.label}</span>
                                                                    {#if !action.state.enabled}
                                                                        <span class="text-xs text-muted-foreground"
                                                                            >{action.state.reason}</span
                                                                        >
                                                                    {/if}
                                                                </span>
                                                            </DropdownMenu.Item>
                                                        {/each}
                                                    </DropdownMenu.Content>
                                                </DropdownMenu.Root>
                                            {/if}
                                        </Table.Cell>
                                    </Table.Row>
                                {/snippet}
                            </ContextMenu.Trigger>
                            <ContextMenu.Content class="w-64">
                                <ContextMenu.Label>{entry.name}</ContextMenu.Label>
                                <ContextMenu.Separator />
                                {#each items as action (action.id)}
                                    <ContextMenu.Item disabled={!action.state.enabled} onSelect={() => act(action)}>
                                        <span class="flex flex-col">
                                            <span>{action.label}</span>
                                            {#if !action.state.enabled}
                                                <span class="text-xs text-muted-foreground">{action.state.reason}</span>
                                            {/if}
                                        </span>
                                    </ContextMenu.Item>
                                {/each}
                            </ContextMenu.Content>
                        </ContextMenu.Root>
                    {/each}
                </Table.Body>
            </Table.Root>
            <div class="flex flex-wrap items-center justify-between gap-2 text-sm text-muted-foreground">
                <span data-testid="page-range"
                    >{total === 0
                        ? "Nothing to show"
                        : `Showing ${listing.offset + 1}–${listing.offset + entries.length} of ${total}`}{listing.truncated
                        ? `; the folder holds more than the ${total} entries read`
                        : ""}</span
                >
                <div class="flex gap-2">
                    <Button size="sm" variant="outline" disabled={offset === 0} onclick={() => (offset = Math.max(0, offset - limit))}
                        >Previous</Button
                    >
                    <Button size="sm" variant="outline" disabled={offset + limit >= total} onclick={() => (offset += limit)}>Next</Button>
                </div>
            </div>
        {/if}

        {#if viewing !== null || viewFailure !== "" || viewBusy}
            <Card.Root>
                <Card.Header class="flex flex-row items-start justify-between gap-4">
                    <div class="space-y-1">
                        <Card.Title class="break-all">{viewing?.name ?? viewingPath}</Card.Title>
                        {#if viewing}
                            <Card.Description>
                                {formatBytes(viewing.size)}, changed {when(viewing.modified_ms)}{viewing.eof
                                    ? ""
                                    : `; showing the first ${formatBytes(viewing.offset + viewing.length)}`}
                            </Card.Description>
                        {/if}
                    </div>
                    <Button variant="ghost" size="icon-sm" aria-label="Close the viewer" onclick={closeViewer}><XIcon /></Button>
                </Card.Header>
                <Card.Content class="space-y-3">
                    {#if viewFailure !== ""}
                        <p class="text-sm text-destructive" role="alert">{viewFailure}</p>
                    {:else if viewing === null}
                        <p class="text-sm text-muted-foreground" aria-busy="true">Reading the file.</p>
                    {:else if viewing.binary}
                        <p class="text-sm text-muted-foreground">This file is not text, so the panel shows none of its bytes.</p>
                    {:else}
                        {#if viewing.redacted}
                            <div
                                class="flex flex-wrap items-center gap-3 rounded-md border border-waiting/30 bg-waiting/5 p-3 text-sm"
                                role="status"
                            >
                                <span>{hiddenLine(viewing.redacted_keys)}</span>
                                {#if may("settings.secrets.read")}
                                    <Button size="sm" variant="outline" disabled={viewBusy} onclick={() => void view(viewingPath, true)}
                                        >Show secrets</Button
                                    >
                                {/if}
                            </div>
                        {:else if viewing.revealed}
                            <p class="rounded-md border border-waiting/30 bg-waiting/5 p-3 text-sm" role="status">
                                Showing {viewing.revealed_keys.length} secret value{viewing.revealed_keys.length === 1 ? "" : "s"}; this
                                reveal is recorded.
                            </p>
                        {/if}
                        <pre
                            class="max-h-96 overflow-auto rounded-md border bg-muted/30 p-3 font-mono text-xs whitespace-pre-wrap"
                            data-testid="file-text">{viewing.text}</pre>
                        {#if viewing.next_offset !== null}
                            <Button size="sm" variant="outline" disabled={viewBusy} onclick={() => void readOn()}>Load more</Button>
                        {/if}
                    {/if}
                </Card.Content>
            </Card.Root>
        {/if}
    </section>
{/if}

<Sheet.Root bind:open={sheetOpen}>
    <Sheet.Content side="bottom" class="max-h-svh overflow-y-auto">
        <Sheet.Header>
            <Sheet.Title>{sheetFor?.kind === "row" ? sheetFor.entry.name : describeSelection(selection, total)}</Sheet.Title>
            <Sheet.Description>Actions this root allows, and why the others wait.</Sheet.Description>
        </Sheet.Header>
        <div class="px-4 pb-4">
            {#if sheetFor?.kind === "row"}
                {@render actionList(rowActions(sheetFor.entry), "sheet-row")}
            {:else if sheetFor?.kind === "mass"}
                {@render actionList(massActions, "sheet-mass")}
            {/if}
        </div>
    </Sheet.Content>
</Sheet.Root>

<Dialog.Root bind:open={rulesOpen}>
    <Dialog.Content>
        <Dialog.Header>
            <Dialog.Title>Protected paths in {root?.label ?? rootId}</Dialog.Title>
            <Dialog.Description>
                One gitignore pattern per line. A path a pattern matches is hidden from listings and refused for reading and every change.
                The built-in protections always hold, and a ! line can only undo one of yours.
            </Dialog.Description>
        </Dialog.Header>
        <div class="space-y-3">
            <div class="space-y-1">
                <Label for="protected-patterns">Your patterns</Label>
                <Textarea id="protected-patterns" class="font-mono text-xs" rows={8} bind:value={rulesText} />
            </div>
            <div class="space-y-1">
                <Label for="protected-reason">Why</Label>
                <Input id="protected-reason" bind:value={rulesReason} maxlength={255} />
            </div>
            {#if rulesFailure !== ""}
                <p class="text-sm text-destructive" role="alert">{rulesFailure}</p>
            {/if}
            {#if builtIn.length > 0}
                <details class="text-sm">
                    <summary class="cursor-pointer text-muted-foreground">Built-in protections ({builtIn.length})</summary>
                    <ul class="mt-2 space-y-1">
                        {#each builtIn as rule, index (index)}
                            <li>
                                <code class="font-mono text-xs">{rule.pattern}</code>
                                <span class="text-muted-foreground">— {rule.why}</span>
                            </li>
                        {/each}
                    </ul>
                </details>
            {/if}
        </div>
        <Dialog.Footer>
            <Button variant="outline" onclick={() => (rulesOpen = false)}>Leave them</Button>
            <Button disabled={rulesBusy} onclick={() => void saveRules()}>Save</Button>
        </Dialog.Footer>
    </Dialog.Content>
</Dialog.Root>
