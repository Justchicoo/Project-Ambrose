<!-- Project Ambrose by Imjustchico: What has been done to an app and by whom, live from its own record, with a panel audit-chain integrity check when the supervisor serves this page. -->
<script lang="ts">
    import * as Card from "$lib/components/ui/card/index.js";
    import * as Table from "$lib/components/ui/table/index.js";
    import { Input } from "$lib/components/ui/input/index.js";
    import { ApiError } from "$lib/api.svelte.js";
    import { live } from "$lib/status.svelte.js";
    import { activityOf, auditChain, candidates, servedBy, supervisorServes } from "$lib/supervision.svelte.js";
    import type { ActivityAnswer, AuditChainAnswer } from "$lib/schemas.js";
    import SearchIcon from "@lucide/svelte/icons/search";
    import PageHeader from "../components/PageHeader.svelte";
    import StatusBadge from "../components/StatusBadge.svelte";

    const choices = $derived(candidates());
    let chosen = $state("");
    let answer = $state<ActivityAnswer | null>(null);
    let chain = $state<AuditChainAnswer | null>(null);
    let chainFailure = $state("");
    let failure = $state("");
    let search = $state("");

    const app = $derived(choices.includes(chosen) ? chosen : (choices[0] ?? ""));
    const showingSupervisor = $derived(app === servedBy() && supervisorServes());

    $effect(() => {
        const name = app;
        const beat = live.now;
        if (name === "") return;
        void beat;
        const controller = new AbortController();
        void (async () => {
            try {
                answer = await activityOf(name, controller.signal);
                failure = "";
            } catch (problem) {
                if (controller.signal.aborted) return;
                answer = null;
                failure = problem instanceof ApiError ? problem.message : "The record could not be read";
            }
        })();
        return () => controller.abort();
    });

    const shown = $derived.by(() => {
        const text = search.trim().toLowerCase();
        return (answer?.activity ?? []).filter(
            (row) =>
                text === "" ||
                row.command.toLowerCase().includes(text) ||
                row.who.toLowerCase().includes(text) ||
                row.address.toLowerCase().includes(text) ||
                row.reason.toLowerCase().includes(text),
        );
    });

    $effect(() => {
        if (!showingSupervisor) {
            chain = null;
            chainFailure = "";
            return;
        }
        const controller = new AbortController();
        void (async () => {
            try {
                chain = await auditChain(controller.signal);
                chainFailure = "";
            } catch (problem) {
                if (controller.signal.aborted) return;
                chain = null;
                chainFailure = problem instanceof ApiError ? problem.message : "The audit chain could not be verified";
            }
        })();
        return () => controller.abort();
    });
</script>

<PageHeader title="Activity" description="Every command this app was sent, and what became of it." />

{#if showingSupervisor}
    <Card.Root class="mb-4">
        <Card.Header>
            <Card.Title>Panel audit chain</Card.Title>
            <Card.Description>
                {#if chainFailure !== ""}
                    {chainFailure}
                {:else if chain === null}
                    Verifying the audit chain…
                {:else if chain.valid}
                    Valid; {chain.rows_checked} row(s) checked in {chain.elapsed_ms} ms.
                {:else}
                    Broken at row {chain.first_invalid_id ?? "unknown"}: {chain.problem}
                {/if}
            </Card.Description>
        </Card.Header>
        <Card.Content class="pt-0 text-sm text-muted-foreground">
            {#if chain?.collector_enabled}
                {chain.pending_events} audit event(s) are waiting for the off-machine collector.
            {:else}
                Collector forwarding is off.
            {/if}
            Verification budget: {chain?.budget_ms ?? 5_000} ms for 10,000 rows. This local chain detects changes and deletions, but cannot prove
            integrity against someone able to rewrite the whole store.
        </Card.Content>
    </Card.Root>
{/if}

{#if failure !== ""}
    <Card.Root class="mb-4">
        <Card.Content class="py-4 text-sm text-destructive">{failure}</Card.Content>
    </Card.Root>
{/if}

<Card.Root>
    <Card.Header class="flex flex-row items-center justify-between gap-4">
        <div>
            <Card.Title>{answer?.written ?? 0} written down</Card.Title>
            <Card.Description>
                Newest first.
                {#if (answer?.unreadable ?? 0) > 0}
                    {answer?.unreadable} line(s) of the record could not be read.
                {/if}
            </Card.Description>
        </div>
        <div class="relative w-56">
            <SearchIcon class="absolute top-1/2 left-2 size-4 -translate-y-1/2 text-muted-foreground" />
            <Input bind:value={search} placeholder="Command, who, address or reason" class="pl-8" />
        </div>
    </Card.Header>
    <Card.Content>
        {#if shown.length === 0}
            <p class="py-6 text-sm text-muted-foreground">
                {answer === null
                    ? "Reading the record."
                    : !answer.kept
                      ? "This app keeps no record yet. One is written the first time a command is sent to it."
                      : search.trim() !== ""
                        ? "Nothing in the record matches that."
                        : "Nothing has been sent to this app yet."}
            </p>
        {:else}
            <Table.Root>
                <Table.Header>
                    <Table.Row>
                        <Table.Head>When</Table.Head>
                        <Table.Head>Command</Table.Head>
                        <Table.Head>Who</Table.Head>
                        <Table.Head class="hidden md:table-cell">From</Table.Head>
                        <Table.Head>Outcome</Table.Head>
                    </Table.Row>
                </Table.Header>
                <Table.Body>
                    {#each shown as row, index (`${row.epoch_ms}-${index}`)}
                        <Table.Row>
                            <Table.Cell class="font-mono text-xs whitespace-nowrap">{row.time}</Table.Cell>
                            <Table.Cell class="font-mono text-sm">{row.command}</Table.Cell>
                            <Table.Cell>{row.who}</Table.Cell>
                            <Table.Cell class="hidden md:table-cell">{row.address}</Table.Cell>
                            <Table.Cell>
                                {#if row.ran}
                                    <StatusBadge tone="healthy">Ran</StatusBadge>
                                {:else}
                                    <StatusBadge tone="wrong">Refused</StatusBadge>
                                    {#if row.reason !== ""}
                                        <span class="ml-2 text-sm text-muted-foreground">{row.reason}</span>
                                    {/if}
                                {/if}
                            </Table.Cell>
                        </Table.Row>
                    {/each}
                </Table.Body>
            </Table.Root>
        {/if}
    </Card.Content>
</Card.Root>
