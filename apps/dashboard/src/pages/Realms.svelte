<!-- Project Ambrose by Imjustchico: The realms a player may be sent to, live from the login server: each realmlist row with where it is reached from outside and from this machine, how full it is against the limit its operator set, and when it last said it was alive, judged by the same rule that decides where a client is sent so the page and the player can never disagree. A realm whose beat is older than the policy allows reads as gone rather than waiting for somebody to mark it down. Editing limits and flags, and the zones a realm has loaded, belong to 17.31 and the milestones it waits on. -->
<script lang="ts">
    import * as Card from "$lib/components/ui/card/index.js";
    import * as Table from "$lib/components/ui/table/index.js";
    import { ApiError } from "$lib/api.svelte.js";
    import { live } from "$lib/status.svelte.js";
    import { realmsOf, servedBy, supervised, supervisorServes } from "$lib/supervision.svelte.js";
    import type { RealmsAnswer } from "$lib/schemas.js";
    import PageHeader from "../components/PageHeader.svelte";
    import StatusBadge from "../components/StatusBadge.svelte";

    const choices = $derived(supervisorServes() ? [servedBy(), ...supervised().map((app) => app.name)] : [servedBy()]);
    let chosen = $state("");
    let answer = $state<RealmsAnswer | null>(null);
    let failure = $state("");

    const app = $derived(choices.includes(chosen) ? chosen : (choices[0] ?? ""));

    $effect(() => {
        const name = app;
        const beat = live.now;
        if (name === "") return;
        void beat;
        const controller = new AbortController();
        void (async () => {
            try {
                answer = await realmsOf(name, controller.signal);
                failure = "";
            } catch (problem) {
                if (controller.signal.aborted) return;
                answer = null;
                failure = problem instanceof ApiError ? problem.message : "The realms could not be read";
            }
        })();
        return () => controller.abort();
    });

    function spell(seconds: number) {
        if (seconds <= 0) return "just now";
        if (seconds < 60) return `${seconds}s ago`;
        const minutes = Math.floor(seconds / 60);
        if (minutes < 60) return `${minutes}m ago`;
        return `${Math.floor(minutes / 60)}h ${minutes % 60}m ago`;
    }

    const realms = $derived(answer?.realms ?? []);
</script>

<PageHeader title="Realms and zones" description="Where a player may be sent, and whether it is still answering." />

{#if failure !== ""}
    <Card.Root class="mb-4">
        <Card.Content class="py-4 text-sm text-destructive">{failure}</Card.Content>
    </Card.Root>
{/if}

<Card.Root>
    <Card.Header>
        <Card.Title>{answer?.online ?? 0} of {answer?.counted ?? 0} answering, {answer?.players ?? 0} player(s)</Card.Title>
        <Card.Description>
            A realm counts as gone once its last beat is older than {(answer?.heartbeat_seconds ?? 30) *
                (answer?.offline_after_intervals ?? 3)} seconds, which is the same rule that decides where a client is sent.
        </Card.Description>
    </Card.Header>
    <Card.Content>
        {#if realms.length === 0}
            <p class="py-6 text-sm text-muted-foreground">
                {answer === null
                    ? "Reading the realms."
                    : "No realm has registered itself yet. A gameserver adds its own row the first time it runs."}
            </p>
        {:else}
            <Table.Root>
                <Table.Header>
                    <Table.Row>
                        <Table.Head>Realm</Table.Head>
                        <Table.Head>State</Table.Head>
                        <Table.Head>Players</Table.Head>
                        <Table.Head class="hidden md:table-cell">Reached at</Table.Head>
                        <Table.Head class="hidden lg:table-cell">On this machine</Table.Head>
                        <Table.Head class="text-right">Last beat</Table.Head>
                    </Table.Row>
                </Table.Header>
                <Table.Body>
                    {#each realms as realm (realm.id)}
                        <Table.Row>
                            <Table.Cell class="font-medium">{realm.name === "" ? `Realm ${realm.id}` : realm.name}</Table.Cell>
                            <Table.Cell>
                                {#if realm.marked_offline}
                                    <StatusBadge tone="wrong">Marked offline</StatusBadge>
                                {:else if !realm.online}
                                    <StatusBadge tone="wrong">Stopped beating</StatusBadge>
                                {:else if realm.full}
                                    <StatusBadge tone="waiting">Full</StatusBadge>
                                {:else}
                                    <StatusBadge tone="healthy" pulse>Answering</StatusBadge>
                                {/if}
                            </Table.Cell>
                            <Table.Cell>{realm.population}{realm.player_limit > 0 ? ` of ${realm.player_limit}` : ""}</Table.Cell>
                            <Table.Cell class="hidden md:table-cell">{realm.address}:{realm.port}</Table.Cell>
                            <Table.Cell class="hidden lg:table-cell">{realm.local_address}:{realm.port}</Table.Cell>
                            <Table.Cell class="text-right">
                                {realm.last_heartbeat_epoch_seconds === 0 ? "never" : spell(realm.heartbeat_age_seconds)}
                            </Table.Cell>
                        </Table.Row>
                    {/each}
                </Table.Body>
            </Table.Root>
        {/if}
    </Card.Content>
</Card.Root>
