<!-- Project Ambrose by Imjustchico: Who is playing right now, live from the login server: every wizard a gameserver has let into a realm, with the account behind it, the realm and zone it is in, its school and level, and how long it has been on, read from the row the handoff writes rather than from anything a client said about itself. A row whose wizard is no longer in the characters database is still shown, named by its id, because an operator needs to see a row that should not be there. The actions a game master will want, kick, mute and teleport, belong to 17.21 and the commands it waits on. -->
<script lang="ts">
    import * as Card from "$lib/components/ui/card/index.js";
    import * as Table from "$lib/components/ui/table/index.js";
    import { Input } from "$lib/components/ui/input/index.js";
    import { ApiError } from "$lib/api.svelte.js";
    import { live } from "$lib/status.svelte.js";
    import { playersOf, servedBy, supervised, supervisorServes } from "$lib/supervision.svelte.js";
    import type { PlayersAnswer } from "$lib/schemas.js";
    import SearchIcon from "@lucide/svelte/icons/search";
    import PageHeader from "../components/PageHeader.svelte";
    import StatusBadge from "../components/StatusBadge.svelte";

    const choices = $derived(supervisorServes() ? [servedBy(), ...supervised().map((app) => app.name)] : [servedBy()]);
    let chosen = $state("");
    let answer = $state<PlayersAnswer | null>(null);
    let failure = $state("");
    let search = $state("");

    const app = $derived(choices.includes(chosen) ? chosen : (choices[0] ?? ""));

    $effect(() => {
        const name = app;
        const beat = live.now;
        if (name === "") return;
        void beat;
        const controller = new AbortController();
        void (async () => {
            try {
                answer = await playersOf(name, controller.signal);
                failure = "";
            } catch (problem) {
                if (controller.signal.aborted) return;
                answer = null;
                failure = problem instanceof ApiError ? problem.message : "Who is online could not be read";
            }
        })();
        return () => controller.abort();
    });

    function spell(seconds: number) {
        if (seconds <= 0) return "just now";
        const hours = Math.floor(seconds / 3600);
        const minutes = Math.floor((seconds % 3600) / 60);
        if (hours > 0) return `${hours}h ${minutes}m`;
        if (minutes > 0) return `${minutes}m`;
        return `${seconds}s`;
    }

    const shown = $derived.by(() => {
        const text = search.trim().toLowerCase();
        return (answer?.players ?? []).filter(
            (player) =>
                text === "" ||
                player.name.toLowerCase().includes(text) ||
                player.account.toLowerCase().includes(text) ||
                player.realm.toLowerCase().includes(text) ||
                player.zone.toLowerCase().includes(text),
        );
    });
</script>

<PageHeader title="Players online" description="Every wizard a gameserver has let into a realm." />

{#if failure !== ""}
    <Card.Root class="mb-4">
        <Card.Content class="py-4 text-sm text-destructive">{failure}</Card.Content>
    </Card.Root>
{/if}

<Card.Root>
    <Card.Header class="flex flex-row items-center justify-between gap-4">
        <div>
            <Card.Title>{answer?.counted ?? 0} online</Card.Title>
            <Card.Description
                >A wizard appears here once its handoff key has been spent, and leaves when its session closes.</Card.Description
            >
        </div>
        <div class="relative w-56">
            <SearchIcon class="absolute top-1/2 left-2 size-4 -translate-y-1/2 text-muted-foreground" />
            <Input bind:value={search} placeholder="Wizard, account, realm or zone" class="pl-8" />
        </div>
    </Card.Header>
    <Card.Content>
        {#if shown.length === 0}
            <p class="py-6 text-sm text-muted-foreground">
                {answer === null
                    ? "Reading who is online."
                    : search.trim() !== ""
                      ? "No wizard matches that."
                      : "Nobody is in a realm right now."}
            </p>
        {:else}
            <Table.Root>
                <Table.Header>
                    <Table.Row>
                        <Table.Head>Wizard</Table.Head>
                        <Table.Head>Account</Table.Head>
                        <Table.Head>Realm</Table.Head>
                        <Table.Head class="hidden md:table-cell">Zone</Table.Head>
                        <Table.Head>Level</Table.Head>
                        <Table.Head class="text-right">On for</Table.Head>
                    </Table.Row>
                </Table.Header>
                <Table.Body>
                    {#each shown as player (player.character_guid)}
                        <Table.Row>
                            <Table.Cell class="font-medium">
                                {#if player.found}
                                    {player.name === "" ? `Wizard ${player.character_guid}` : player.name}
                                {:else}
                                    <StatusBadge tone="wrong">Wizard {player.character_guid} is gone</StatusBadge>
                                {/if}
                            </Table.Cell>
                            <Table.Cell>{player.account === "" ? `Account ${player.account_id}` : player.account}</Table.Cell>
                            <Table.Cell>{player.realm === "" ? `Realm ${player.realm_id}` : player.realm}</Table.Cell>
                            <Table.Cell class="hidden md:table-cell">{player.zone}</Table.Cell>
                            <Table.Cell>{player.found ? player.level : ""}</Table.Cell>
                            <Table.Cell class="text-right">{spell(player.seconds)}</Table.Cell>
                        </Table.Row>
                    {/each}
                </Table.Body>
            </Table.Root>
        {/if}
    </Card.Content>
</Card.Root>
