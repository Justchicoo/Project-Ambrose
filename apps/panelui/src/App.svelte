<!-- Project Ambrose by Imjustchico: The panel program's own screens: the list of panels with This computer at its head and each entry's state beside it as a colour and a word, adding a panel by its address with its certificate shown for comparing before it is pinned, pairing from the line a supervisor printed, and the about screen. Each panel opens in a window of its own, from its own origin; nothing here asks for a password, because a panel's own sign-in page is where one is typed. Every screen is reached from the keyboard and every control carries its label. -->
<script lang="ts">
    import { AppShell, Button, Card, Checkbox, createHost, Heading, IconButton, Mono, StateDot, TextField } from "@ambrose/ui";
    import { hostChannel, ThisComputerId } from "./lib/channel";
    import { PanelsState } from "./lib/panels.svelte";

    const { panels = new PanelsState(hostChannel(createHost())) }: { panels?: PanelsState } = $props();

    let address = $state("");
    let name = $state("");
    let plain = $state(false);
    let confirmed = $state(false);
    let line = $state("");
    let pairName = $state("");

    let watching = false;
    $effect(() => {
        if (watching) return;
        watching = true;
        void panels.watch();
    });

    function schemeOf(text: string): string {
        try {
            return new URL(text.trim()).protocol;
        } catch {
            return "";
        }
    }

    function dot(id: number): "healthy" | "waiting" | "wrong" | "unknown" {
        const state = panels.states[id]?.state;
        if (state === "answering") return "healthy";
        if (state === "certificate changed" || state === "not an Ambrose panel") return "wrong";
        if (state === "unreachable") return "waiting";
        return "unknown";
    }

    async function look(event: Event) {
        event.preventDefault();
        confirmed = false;
        if (schemeOf(address) === "https:") await panels.inspect(address);
    }

    async function add(event: Event) {
        event.preventDefault();
        if (await panels.add(name === "" ? address : name, address, plain, confirmed)) {
            address = "";
            name = "";
            plain = false;
            confirmed = false;
        }
    }

    async function pair(event: Event) {
        event.preventDefault();
        if (await panels.pair(line.trim(), pairName)) {
            line = "";
            pairName = "";
        }
    }
</script>

<AppShell product="Ambrose Panel">
    {#snippet barEnd()}
        {#if panels.screen === "list"}
            <IconButton icon="info" label="About" onclick={() => panels.showAbout()} />
        {:else}
            <Button variant="quiet" onclick={() => panels.go("list")}>Back</Button>
        {/if}
    {/snippet}

    <section class="flex min-h-0 flex-1 flex-col gap-24 overflow-auto p-40" data-ambrose-scroll>
        {#if panels.problem !== ""}
            <p role="alert" class="text-15 text-state-wrong">{panels.problem}</p>
        {/if}

        {#if panels.screen === "list"}
            <Heading level={1} size="34">Panels</Heading>
            <Card title="This computer">
                {#if panels.listing?.this_computer.found}
                    <div class="flex flex-wrap items-center justify-between gap-16">
                        <div class="flex flex-col gap-4">
                            <StateDot state="healthy" word="The supervisor here answers" />
                            <span class="text-13 text-fg-muted">Revision <Mono>{panels.listing.this_computer.revision}</Mono></span>
                        </div>
                        <Button variant="action" busy={panels.busy} busyLabel="Opening" onclick={() => panels.open(ThisComputerId)}
                            >Open This computer</Button
                        >
                    </div>
                {:else}
                    <div class="flex flex-col gap-4">
                        <StateDot state="unknown" word="No Ambrose server is running on this computer" />
                        {#if panels.listing?.this_computer.word}
                            <span class="text-13 text-fg-muted">{panels.listing.this_computer.word}</span>
                        {/if}
                    </div>
                {/if}
            </Card>

            <Card title="Other panels">
                {#snippet actions()}
                    <Button variant="quiet" icon="key" onclick={() => panels.go("pair")}>Pair</Button>
                    <Button variant="quiet" icon="plus" onclick={() => panels.go("add")}>Add</Button>
                {/snippet}
                {#if !panels.listing || panels.listing.panels.length === 0}
                    <p class="text-15 text-fg-muted">
                        No other panels yet. Pair one from the line its supervisor prints, or add one by its address.
                    </p>
                {:else}
                    <ul class="flex flex-col gap-12">
                        {#each panels.listing.panels as panel (panel.id)}
                            <li class="flex flex-wrap items-center justify-between gap-16 border-t border-edge-quiet pt-12">
                                <div class="flex flex-col gap-4">
                                    <span class="text-15">{panel.name}</span>
                                    <Mono>{panel.origin}</Mono>
                                    <StateDot state={dot(panel.id)} word={panels.states[panel.id]?.word ?? "Not asked yet"} />
                                    {#if panel.warning !== ""}
                                        <span class="text-13 text-state-waiting">{panel.warning}</span>
                                    {/if}
                                </div>
                                <div class="flex items-center gap-8">
                                    <Button variant="action" onclick={() => panels.open(panel.id)}>Open {panel.name}</Button>
                                    <Button variant="danger" onclick={() => panels.forget(panel.id)}>Forget {panel.name}</Button>
                                </div>
                            </li>
                        {/each}
                    </ul>
                {/if}
            </Card>
        {:else if panels.screen === "add"}
            <Heading level={1} size="34">Add a panel</Heading>
            <div style="max-width: 36rem">
                <Card title="Its address">
                    <form class="flex flex-col gap-16" onsubmit={add}>
                        <TextField
                            id="panel-address"
                            label="Address"
                            bind:value={address}
                            placeholder="The address its supervisor printed, with its port"
                            mono
                        />
                        <TextField id="panel-name" label="Name" bind:value={name} placeholder="Realm one" />
                        <Button variant="quiet" type="button" onclick={look}>Show its certificate</Button>
                        {#if panels.inspection?.served}
                            <p class="text-13 text-fg-muted">
                                It serves <Mono>{panels.inspection.served}</Mono>. Compare it with the fingerprint the supervisor printed
                                when it made its certificate.
                            </p>
                            <Checkbox id="panel-confirmed" label="The fingerprints match" bind:checked={confirmed} />
                        {/if}
                        {#if schemeOf(address) === "http:"}
                            <Checkbox
                                id="panel-plain"
                                label="Reach it over plain HTTP"
                                hint="The password and the session then cross the network unencrypted"
                                bind:checked={plain}
                            />
                        {/if}
                        <div class="flex items-center gap-12">
                            <Button variant="quiet" type="button" onclick={() => panels.go("list")}>Cancel</Button>
                            <Button variant="action" type="submit">Add this panel</Button>
                        </div>
                    </form>
                </Card>
            </div>
        {:else if panels.screen === "pair"}
            <Heading level={1} size="34">Pair a panel</Heading>
            <div style="max-width: 36rem">
                <Card title="The line its supervisor printed">
                    <form class="flex flex-col gap-16" onsubmit={pair}>
                        <TextField id="panel-line" label="Pairing line" bind:value={line} placeholder="The whole line, as printed" mono />
                        <TextField id="panel-pair-name" label="Name" bind:value={pairName} placeholder="Realm one" />
                        <div class="flex items-center gap-12">
                            <Button variant="quiet" type="button" onclick={() => panels.go("list")}>Cancel</Button>
                            <Button variant="action" type="submit">Pair and open</Button>
                        </div>
                    </form>
                </Card>
            </div>
        {:else if panels.screen === "about"}
            <Heading level={1} size="34">About</Heading>
            <Card title="This program">
                {#if panels.about}
                    <dl class="grid grid-cols-2 gap-8 text-15">
                        <dt>Version</dt>
                        <dd><Mono>{panels.about.version}</Mono></dd>
                        <dt>Revision</dt>
                        <dd><Mono>{panels.about.revision}</Mono></dd>
                        <dt>Web view</dt>
                        <dd><Mono>{panels.about.webview}</Mono></dd>
                        <dt>This computer's supervisor</dt>
                        <dd><Mono>{panels.about.supervisor_revision || "none answers"}</Mono></dd>
                    </dl>
                {/if}
            </Card>
        {/if}
    </section>
</AppShell>
