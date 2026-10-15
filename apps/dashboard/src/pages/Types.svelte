<!-- Project Ambrose by Imjustchico: The type registry browser: every class the server's type dump holds, with its bases, derived classes, properties, flags and enum options. -->
<script lang="ts">
    import * as Card from "$lib/components/ui/card/index.js";
    import * as Table from "$lib/components/ui/table/index.js";
    import { Input } from "$lib/components/ui/input/index.js";
    import { Button } from "$lib/components/ui/button/index.js";
    import { ApiError, request } from "$lib/api.svelte.js";
    import { live } from "$lib/status.svelte.js";
    import { SvelteURLSearchParams } from "svelte/reactivity";
    import { candidates, pathFor } from "$lib/supervision.svelte.js";
    import type { TypeDetailAnswer, TypeListAnswer, TypeStamp, CapabilitiesAnswer } from "$lib/schemas.js";
    import {
        TypeListAnswer as TypeListSchema,
        TypeDetailAnswer as TypeDetailSchema,
        CapabilitiesAnswer as CapabilitiesSchema,
    } from "$lib/schemas.js";
    import { ObjectTree, type TreeNode } from "@ambrose/ui";
    import SearchIcon from "@lucide/svelte/icons/search";
    import PageHeader from "../components/PageHeader.svelte";

    const choices = $derived(candidates());
    let chosen = $state("");
    let search = $state("");
    let debounced = $state("");
    let list = $state<TypeListAnswer | null>(null);
    let detail = $state<TypeDetailAnswer | null>(null);
    let stamp = $state<TypeStamp | null>(null);
    let failure = $state("");
    let detailFailure = $state("");
    let loading = $state(false);
    let cursor = $state<string | null>(null);
    let kinds = $state<Array<{ kind: string; generation: number }>>([]);

    const app = $derived(choices.includes(chosen) ? chosen : (choices[0] ?? ""));

    $effect(() => {
        const timer = setTimeout(() => {
            debounced = search;
        }, 300);
        return () => clearTimeout(timer);
    });

    async function loadKinds(name: string, signal: AbortSignal): Promise<void> {
        try {
            const answer: CapabilitiesAnswer = await request("GET", pathFor(name, "capabilities"), CapabilitiesSchema, undefined, signal);
            kinds = answer.data_kinds ?? [];
        } catch {
            kinds = [];
        }
    }

    async function loadList(name: string, query: string, next: string | null, signal: AbortSignal): Promise<void> {
        loading = true;
        try {
            const params = new SvelteURLSearchParams();
            if (query.trim() !== "") params.set("q", query.trim());
            params.set("limit", "200");
            if (next) params.set("cursor", next);
            const answer: TypeListAnswer = await request("GET", pathFor(name, `data/types?${params}`), TypeListSchema, undefined, signal);
            list = next ? { ...answer, rows: [...(list?.rows ?? []), ...answer.rows] } : answer;
            stamp = answer.stamp;
            cursor = answer.has_more ? (answer.cursor ?? null) : null;
            failure = "";
        } catch (problem) {
            if (signal.aborted) return;
            list = null;
            failure = problem instanceof ApiError ? problem.message : "The type list could not be read";
        } finally {
            loading = false;
        }
    }

    async function loadDetail(name: string, className: string, signal: AbortSignal): Promise<void> {
        try {
            const answer: TypeDetailAnswer = await request(
                "GET",
                pathFor(name, `data/types/${encodeURIComponent(className)}`),
                TypeDetailSchema,
                undefined,
                signal,
            );
            detail = answer;
            stamp = answer.stamp;
            detailFailure = "";
        } catch (problem) {
            if (signal.aborted) return;
            detail = null;
            detailFailure = problem instanceof ApiError ? problem.message : "The class could not be read";
        }
    }

    $effect(() => {
        const name = app;
        const query = debounced;
        const beat = live.now;
        if (name === "") return;
        void beat;
        const controller = new AbortController();
        void loadKinds(name, controller.signal);
        void loadList(name, query, null, controller.signal);
        detail = null;
        return () => controller.abort();
    });

    function openClass(name: string): void {
        const appName = app;
        if (appName === "") return;
        const controller = new AbortController();
        void loadDetail(appName, name, controller.signal);
    }

    function goBack(): void {
        detail = null;
    }

    const treeNodes = $derived.by((): TreeNode[] => {
        if (!detail) return [];
        return detail.properties.map((p) => ({
            name: p.name,
            type: p.type,
            detail: `id ${p.id} · offset ${p.offset} · hash ${p.hash}`,
            flags: p.flags,
            enumOptions: p.enum_options,
        }));
    });
</script>

<PageHeader title="Types" description="Every class the type registry holds, with its bases, properties, flags and enum options." />

{#if stamp}
    <p class="mb-4 font-mono text-12 text-fg-muted">
        Revision {stamp.revision || "unknown"} · program {stamp.program_sha256 ? `${stamp.program_sha256.slice(0, 12)}…` : "unknown"} · generation
        {stamp.generation}
    </p>
{/if}

{#if detail === null}
    <Card.Root>
        <Card.Header class="flex flex-row items-center justify-between gap-4">
            <div>
                <Card.Title>{list?.total ?? 0} classes</Card.Title>
                <Card.Description>
                    {#if kinds.length > 0}
                        Kinds: {kinds.map((k) => `${k.kind} (gen ${k.generation})`).join(", ")}.
                    {/if}
                    Search by name or hash.
                </Card.Description>
            </div>
            <div class="relative w-64">
                <SearchIcon class="absolute top-1/2 left-2 size-4 -translate-y-1/2 text-muted-foreground" />
                <Input bind:value={search} placeholder="Class name or hash" class="pl-8" aria-label="Search classes" />
            </div>
        </Card.Header>
        <Card.Content>
            {#if failure !== ""}
                <p class="py-6 text-sm text-destructive">{failure}</p>
            {:else if list === null}
                <p class="py-6 text-sm text-muted-foreground">Reading the type registry.</p>
            {:else if list.rows.length === 0}
                <p class="py-6 text-sm text-muted-foreground">Nothing in the registry matches that.</p>
            {:else}
                <Table.Root>
                    <Table.Header>
                        <Table.Row>
                            <Table.Head>Class</Table.Head>
                            <Table.Head>Kind</Table.Head>
                            <Table.Head>Properties</Table.Head>
                            <Table.Head>Source</Table.Head>
                        </Table.Row>
                    </Table.Header>
                    <Table.Body>
                        {#each list.rows as row (row.hash)}
                            <Table.Row>
                                <Table.Cell>
                                    <button
                                        type="button"
                                        onclick={() => openClass(row.name)}
                                        class="font-mono text-sm text-fill-action underline decoration-fill-action/40 underline-offset-2 hover:decoration-fill-action"
                                    >
                                        {row.name}
                                    </button>
                                    <span class="ml-2 font-mono text-12 text-fg-muted">{row.hash}</span>
                                </Table.Cell>
                                <Table.Cell class="font-mono text-12">{row.kind}</Table.Cell>
                                <Table.Cell class="font-mono text-12">{row.property_count}</Table.Cell>
                                <Table.Cell class="font-mono text-12">{row.from_server_class ? "server_class" : "type dump"}</Table.Cell>
                            </Table.Row>
                        {/each}
                    </Table.Body>
                </Table.Root>
                {#if cursor}
                    <div class="mt-4 flex justify-center">
                        <Button
                            variant="outline"
                            disabled={loading}
                            onclick={() => {
                                const name = app;
                                if (name === "") return;
                                const controller = new AbortController();
                                void loadList(name, debounced, cursor, controller.signal);
                            }}
                        >
                            {loading ? "Loading…" : "Show more"}
                        </Button>
                    </div>
                {/if}
            {/if}
        </Card.Content>
    </Card.Root>
{:else}
    <div class="mb-4">
        <Button variant="outline" onclick={goBack}>← All classes</Button>
    </div>
    {#if detailFailure !== ""}
        <Card.Root>
            <Card.Content class="py-4 text-sm text-destructive">{detailFailure}</Card.Content>
        </Card.Root>
    {:else}
        <Card.Root class="mb-4">
            <Card.Header>
                <Card.Title class="font-mono">{detail.name}</Card.Title>
                <Card.Description>
                    Hash {detail.hash} · {detail.kind} · {detail.property_count} properties · from {detail.provenance === "server_class"
                        ? "the world database's server_class tables"
                        : "the type dump"}
                </Card.Description>
            </Card.Header>
            <Card.Content>
                {#if detail.bases.length > 0}
                    <p class="mb-2 text-sm">
                        <span class="text-muted-foreground">Bases:</span>
                        {#each detail.bases as base, i (base)}
                            {#if i > 0}<span class="text-muted-foreground">, </span>{/if}<button
                                type="button"
                                onclick={() => openClass(base)}
                                class="font-mono text-sm text-fill-action underline decoration-fill-action/40 underline-offset-2 hover:decoration-fill-action"
                                >{base}</button
                            >
                        {/each}
                    </p>
                {/if}
                {#if detail.derived.length > 0}
                    <p class="mb-2 text-sm">
                        <span class="text-muted-foreground">Derived:</span>
                        {#each detail.derived as derived, i (derived)}
                            {#if i > 0}<span class="text-muted-foreground">, </span>{/if}<button
                                type="button"
                                onclick={() => openClass(derived)}
                                class="font-mono text-sm text-fill-action underline decoration-fill-action/40 underline-offset-2 hover:decoration-fill-action"
                                >{derived}</button
                            >
                        {/each}
                    </p>
                {/if}
                {#if detail.typed_views.length > 0}
                    <p class="mb-2 font-mono text-12 text-muted-foreground">Typed views: {detail.typed_views.join(", ")}</p>
                {/if}
            </Card.Content>
        </Card.Root>
        <Card.Root>
            <Card.Header>
                <Card.Title>Properties</Card.Title>
            </Card.Header>
            <Card.Content>
                <ObjectTree nodes={treeNodes} />
            </Card.Content>
        </Card.Root>
    {/if}
{/if}
