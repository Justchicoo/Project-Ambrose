<!-- Project Ambrose by Imjustchico: What client data an app is running on, live from the app itself: the install it found and whether that revision is the one the project pins, the type dump in use with the revision, executable hash and extractor that made it and whether it was built at this start, and how many message definitions were read from the client. It shows facts about the install rather than anything out of it, so no path here is a link and no client file's bytes are ever fetched. Rebuilding the data and switching an app to another install belong to 17.20 and wait on the setup path 3.23 owns. -->
<script lang="ts">
    import * as Card from "$lib/components/ui/card/index.js";
    import * as Table from "$lib/components/ui/table/index.js";
    import { Button } from "$lib/components/ui/button/index.js";
    import { ApiError } from "$lib/api.svelte.js";
    import { clientDataOf, servedBy, supervised, supervisorServes } from "$lib/supervision.svelte.js";
    import type { ClientAnswer } from "$lib/schemas.js";
    import PageHeader from "../components/PageHeader.svelte";
    import StatusBadge from "../components/StatusBadge.svelte";

    const choices = $derived(supervisorServes() ? [servedBy(), ...supervised().map((app) => app.name)] : [servedBy()]);
    let chosen = $state("");
    let answer = $state<ClientAnswer | null>(null);
    let failure = $state("");

    const app = $derived(choices.includes(chosen) ? chosen : (choices[0] ?? ""));

    $effect(() => {
        const name = app;
        if (name === "") return;
        const controller = new AbortController();
        void (async () => {
            try {
                answer = await clientDataOf(name, controller.signal);
                failure = "";
            } catch (problem) {
                if (controller.signal.aborted) return;
                answer = null;
                failure = problem instanceof ApiError ? problem.message : "The client data could not be read";
            }
        })();
        return () => controller.abort();
    });

    const install = $derived(answer?.install ?? null);
    const dump = $derived(answer?.type_dump ?? null);
</script>

<PageHeader title="Client data" description="The install and type data this app is running on." />

{#if choices.length > 1}
    <div class="mb-4 flex flex-wrap gap-2">
        {#each choices as name (name)}
            <Button variant={name === app ? "default" : "outline"} size="sm" onclick={() => (chosen = name)}>{name}</Button>
        {/each}
    </div>
{/if}

{#if failure !== ""}
    <Card.Root class="mb-4">
        <Card.Content class="py-4 text-sm text-destructive">{failure}</Card.Content>
    </Card.Root>
{/if}

<div class="grid gap-4 lg:grid-cols-2">
    <Card.Root>
        <Card.Header>
            <Card.Title>Install</Card.Title>
            <Card.Action>
                {#if install === null}
                    <StatusBadge tone="unknown">Reading</StatusBadge>
                {:else if !install.found}
                    <StatusBadge tone="wrong">Not found</StatusBadge>
                {:else if install.pinned}
                    <StatusBadge tone="healthy">Pinned revision</StatusBadge>
                {:else}
                    <StatusBadge tone="waiting">Another revision</StatusBadge>
                {/if}
            </Card.Action>
        </Card.Header>
        <Card.Content>
            {#if install === null}
                <p class="text-sm text-muted-foreground">Reading what this app found.</p>
            {:else if !install.found}
                <p class="text-sm text-muted-foreground">
                    This app is running with no Wizard101 install, so client messages are logged by service and order only.
                </p>
            {:else}
                <Table.Root>
                    <Table.Body>
                        <Table.Row
                            ><Table.Cell class="text-muted-foreground">Revision</Table.Cell><Table.Cell class="font-mono text-sm"
                                >{install.revision}</Table.Cell
                            ></Table.Row
                        >
                        <Table.Row
                            ><Table.Cell class="text-muted-foreground">Pinned</Table.Cell><Table.Cell>{answer?.pinned_revision}</Table.Cell
                            ></Table.Row
                        >
                        <Table.Row
                            ><Table.Cell class="text-muted-foreground">Folder</Table.Cell><Table.Cell class="font-mono text-xs break-all"
                                >{install.root}</Table.Cell
                            ></Table.Row
                        >
                        <Table.Row
                            ><Table.Cell class="text-muted-foreground">Program</Table.Cell><Table.Cell
                                >{install.has_program ? "present" : "not there"}</Table.Cell
                            ></Table.Row
                        >
                    </Table.Body>
                </Table.Root>
            {/if}
        </Card.Content>
    </Card.Root>

    <Card.Root>
        <Card.Header>
            <Card.Title>Type dump</Card.Title>
            <Card.Action>
                {#if dump === null}
                    <StatusBadge tone="unknown">Reading</StatusBadge>
                {:else if !dump.found}
                    <StatusBadge tone="wrong">Not there</StatusBadge>
                {:else if !dump.readable}
                    <StatusBadge tone="wrong">Unreadable</StatusBadge>
                {:else if !dump.matches_install}
                    <StatusBadge tone="waiting">Another revision</StatusBadge>
                {:else}
                    <StatusBadge tone="healthy">Matches the install</StatusBadge>
                {/if}
            </Card.Action>
        </Card.Header>
        <Card.Content>
            {#if dump === null}
                <p class="text-sm text-muted-foreground">Reading the type dump.</p>
            {:else if !dump.found}
                <p class="text-sm text-destructive">{dump.error === "" ? "This app has no type dump in use." : dump.error}</p>
            {:else}
                <Table.Root>
                    <Table.Body>
                        <Table.Row
                            ><Table.Cell class="text-muted-foreground">Revision</Table.Cell><Table.Cell class="font-mono text-sm"
                                >{dump.revision}</Table.Cell
                            ></Table.Row
                        >
                        <Table.Row
                            ><Table.Cell class="text-muted-foreground">Made by</Table.Cell><Table.Cell>{dump.extractor}</Table.Cell
                            ></Table.Row
                        >
                        <Table.Row
                            ><Table.Cell class="text-muted-foreground">Built this start</Table.Cell><Table.Cell
                                >{dump.built_now ? "yes" : "no, it was already there"}</Table.Cell
                            ></Table.Row
                        >
                        <Table.Row
                            ><Table.Cell class="text-muted-foreground">Program hash</Table.Cell><Table.Cell
                                class="font-mono text-xs break-all">{dump.executable_sha256}</Table.Cell
                            ></Table.Row
                        >
                    </Table.Body>
                </Table.Root>
            {/if}
        </Card.Content>
    </Card.Root>

    <Card.Root class="lg:col-span-2">
        <Card.Header>
            <Card.Title>Message definitions</Card.Title>
            <Card.Action>
                {#if answer === null}
                    <StatusBadge tone="unknown">Reading</StatusBadge>
                {:else if answer.messages.loaded}
                    <StatusBadge tone="healthy">{answer.messages.counted} loaded</StatusBadge>
                {:else}
                    <StatusBadge tone="wrong">None loaded</StatusBadge>
                {/if}
            </Card.Action>
        </Card.Header>
        <Card.Content>
            <p class="text-sm text-muted-foreground">
                {#if answer === null}
                    Reading what this app loaded.
                {:else if answer.messages.loaded}
                    Read from the client's own archive at this app's last start, which is what lets a message be named rather than logged by
                    service and order.
                {:else}
                    This app loaded no message definitions, so a client message is logged by its service and order only.
                {/if}
            </p>
        </Card.Content>
    </Card.Root>
</div>
