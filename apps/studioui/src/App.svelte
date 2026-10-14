<!-- Project Ambrose by Imjustchico: The Studio window, built for a 1440 by 900 window: a header naming the product with what was loaded, quests, goals, results and cinematics, or why something is missing; a switch between Quests and Cinematics; a searchable list paged by the Studio; and the chosen item beside it. A quest shows its title, level and flags, the quests before and after it, its goals with their decoded requirements, its dialog lines in order with their text, and its results, start and completion, each decoded, with any cinematic a result plays as a link that opens it. A cinematic shows what it was decoded from and every quest result that plays it, each a link back to that quest, above its decoded stages, actors and actions. Every control carries its label and is reached from the keyboard alone. -->
<script lang="ts">
    import { Badge, Button, createHost, Heading, Mono, TextField } from "@ambrose/ui";
    import {
        hostChannel,
        type Cinematic,
        type CinematicItem,
        type Page,
        type Quest,
        type QuestItem,
        type StudioChannel,
        type Summary,
    } from "./lib/studio";

    const { channel = hostChannel(createHost()) }: { channel?: StudioChannel } = $props();

    type View = "quests" | "cinematics";
    const PageSize = 100;

    let view: View = $state("quests");
    let search = $state("");
    let offset = $state(0);
    let summary: Summary | null = $state(null);
    let quests: Page<QuestItem> = $state({ total: 0, offset: 0, items: [] });
    let cinematics: Page<CinematicItem> = $state({ total: 0, offset: 0, items: [] });
    let quest: Quest | null = $state(null);
    let cinematic: Cinematic | null = $state(null);
    let missing = $state("");

    const total = $derived(view === "quests" ? quests.total : cinematics.total);

    async function load(): Promise<void> {
        if (view === "quests") quests = await channel.quests({ search, offset, limit: PageSize });
        else cinematics = await channel.cinematics({ search, offset, limit: PageSize });
    }

    let started = false;
    $effect(() => {
        if (started) return;
        started = true;
        void (async () => {
            summary = await channel.summary();
            await load();
        })();
    });

    function show(next: View): void {
        view = next;
        offset = 0;
        void load();
    }

    function searched(): void {
        offset = 0;
        void load();
    }

    function turn(by: number): void {
        offset = Math.max(0, offset + by);
        void load();
    }

    async function openQuest(name: string): Promise<void> {
        missing = "";
        cinematic = null;
        quest = await channel.quest(name);
        if (!quest) missing = `No quest is named ${name}.`;
    }

    async function openCinematic(name: string): Promise<void> {
        missing = "";
        quest = null;
        cinematic = await channel.cinematic(name);
        if (!cinematic) missing = `No cinematic is named ${name}.`;
    }

    function pretty(value: unknown): string {
        return value === null || value === undefined ? "none" : JSON.stringify(value, null, 2);
    }

    function shortName(name: string): string {
        return name.replace(/^Cinematics\//i, "").replace(/\.xml$/i, "");
    }
</script>

<div class="flex h-screen flex-col bg-surface-page text-fg-body">
    <header class="flex items-center justify-between gap-16 border-b border-edge-quiet bg-surface-chrome px-20 py-12">
        <Heading level={1}>Ambrose Studio</Heading>
        {#if summary}
            <p class="text-13 text-fg-muted" aria-live="polite">
                {summary.quests} quests, {summary.goals} goals, {summary.results} results, {summary.cinematics} cinematics ({summary.linked_cinematics}
                played by a quest)
                {#if !summary.install}<span class="text-state-wrong"> · no install found</span>{/if}
                {#if !summary.types}<span class="text-state-wrong"> · no type dump, nothing decoded</span>{/if}
            </p>
        {/if}
    </header>

    <div class="grid min-h-0 flex-1 grid-cols-[22rem_1fr]">
        <section class="flex min-h-0 flex-col border-r border-edge-quiet bg-surface-card" aria-label="Browse">
            <div class="flex gap-8 p-12" role="group" aria-label="What to browse">
                <Button variant={view === "quests" ? "action" : "quiet"} onclick={() => show("quests")}>Quests</Button>
                <Button variant={view === "cinematics" ? "action" : "quiet"} onclick={() => show("cinematics")}>Cinematics</Button>
            </div>
            <form
                class="px-12"
                onsubmit={(event) => {
                    event.preventDefault();
                    searched();
                }}
            >
                <TextField
                    id="studio-search"
                    label={view === "quests" ? "Find a quest by name or title" : "Find a cinematic by name"}
                    type="search"
                    bind:value={search}
                />
            </form>
            <ul class="mt-8 min-h-0 flex-1 overflow-y-auto" aria-label={view === "quests" ? "Quests" : "Cinematics"}>
                {#if view === "quests"}
                    {#each quests.items as item (item.name)}
                        <li>
                            <button
                                class="w-full border-b border-edge-quiet/40 px-12 py-8 text-start hover:bg-surface-sunken focus-visible:bg-surface-sunken"
                                class:bg-surface-sunken={quest?.name === item.name}
                                onclick={() => void openQuest(item.name)}
                            >
                                <span class="block truncate">{item.title || item.name}</span>
                                <span class="block truncate text-12 text-fg-muted"
                                    >{item.name} · level {item.level} · {item.goals} goals{item.mainline ? " · main" : ""}</span
                                >
                            </button>
                        </li>
                    {/each}
                {:else}
                    {#each cinematics.items as item (item.name)}
                        <li>
                            <button
                                class="w-full border-b border-edge-quiet/40 px-12 py-8 text-start hover:bg-surface-sunken focus-visible:bg-surface-sunken"
                                class:bg-surface-sunken={cinematic?.name === item.name}
                                onclick={() => void openCinematic(item.name)}
                            >
                                <span class="block truncate">{shortName(item.name)}</span>
                                {#if item.used_by > 0}<span class="block text-12 text-state-healthy"
                                        >played by {item.used_by} quest result{item.used_by === 1 ? "" : "s"}</span
                                    >{/if}
                            </button>
                        </li>
                    {/each}
                {/if}
            </ul>
            <div class="flex items-center justify-between gap-8 border-t border-edge-quiet p-12 text-13 text-fg-muted">
                <Button variant="quiet" disabled={offset === 0} onclick={() => turn(-PageSize)}>Previous</Button>
                <span>{total === 0 ? "nothing found" : `${offset + 1} to ${Math.min(offset + PageSize, total)} of ${total}`}</span>
                <Button variant="quiet" disabled={offset + PageSize >= total} onclick={() => turn(PageSize)}>Next</Button>
            </div>
        </section>

        <main class="min-h-0 overflow-y-auto p-24" aria-live="polite">
            {#if missing}
                <p class="text-state-wrong">{missing}</p>
            {:else if quest}
                <article class="space-y-84">
                    <div>
                        <Heading level={2}>{quest.title || quest.name}</Heading>
                        <p class="text-13 text-fg-muted">
                            <Mono>{quest.name}</Mono> · level {quest.level} · giver template {quest.giver || "none"}
                        </p>
                        <div class="mt-8 flex flex-wrap gap-8">
                            {#if quest.mainline}<Badge tone="mine">Main line</Badge>{/if}
                            {#if quest.hidden}<Badge>Hidden</Badge>{/if}
                            {#if quest.pet_only}<Badge>Pet only</Badge>{/if}
                            {#if quest.repeat > 0}<Badge>Repeats {quest.repeat}</Badge>{/if}
                        </div>
                        {#if quest.info}<p class="mt-12 max-w-prose">{quest.info}</p>{/if}
                    </div>

                    <section>
                        <Heading level={3}>Chain</Heading>
                        <p class="text-13">
                            Before:
                            {#each quest.prequests as name (name)}<button class="me-8 underline" onclick={() => void openQuest(name)}
                                    >{name}</button
                                >{:else}none{/each}
                        </p>
                        <p class="text-13">
                            After:
                            {#each quest.followups as name (name)}<button class="me-8 underline" onclick={() => void openQuest(name)}
                                    >{name}</button
                                >{:else}none{/each}
                        </p>
                    </section>

                    <section>
                        <Heading level={3}>Goals ({quest.goals.length})</Heading>
                        {#each quest.goals as goal (goal.name)}
                            <details class="mt-8 rounded-card border border-edge-quiet bg-surface-card p-12">
                                <summary class="cursor-pointer"
                                    >{goal.title || goal.name}
                                    <span class="text-12 text-fg-muted"
                                        >{goal.type}{goal.destination_zone ? ` · ${goal.destination_zone}` : ""}{goal.persona
                                            ? ` · ${goal.persona}`
                                            : ""}</span
                                    ></summary
                                >
                                <pre class="mt-8 overflow-x-auto text-12">{pretty(goal.requirements)}</pre>
                            </details>
                        {/each}
                    </section>

                    <section>
                        <Heading level={3}>Dialog ({quest.dialogs.length} lines)</Heading>
                        <ol class="mt-8 space-y-8">
                            {#each quest.dialogs as line, index (index)}
                                <li class="rounded-card border border-edge-quiet bg-surface-card p-12">
                                    <p class="text-12 text-fg-muted">
                                        {line.tag}{line.goal ? ` · ${line.goal}` : ""} · actor {line.actor}{line.event
                                            ? ` · event ${line.event}`
                                            : ""}
                                    </p>
                                    <p>{line.text || line.key}</p>
                                </li>
                            {/each}
                        </ol>
                    </section>

                    <section>
                        <Heading level={3}>Results and rewards ({quest.results.length})</Heading>
                        {#each quest.results as result, index (index)}
                            <details class="mt-8 rounded-card border border-edge-quiet bg-surface-card p-12">
                                <summary class="cursor-pointer">
                                    {result.class}
                                    <span class="text-12 text-fg-muted"
                                        >{result.on_start ? "on accept" : "on complete"}{result.goal ? ` · goal ${result.goal}` : ""}</span
                                    >
                                    {#each result.cinematics as name (name)}<button
                                            class="ms-8 text-13 text-state-healthy underline"
                                            onclick={() => void openCinematic(name)}>plays {shortName(name)}</button
                                        >{/each}
                                </summary>
                                <pre class="mt-8 overflow-x-auto text-12">{pretty(result.data)}</pre>
                            </details>
                        {/each}
                    </section>

                    <section>
                        <Heading level={3}>Requirements</Heading>
                        <pre class="mt-8 overflow-x-auto rounded-card border border-edge-quiet bg-surface-card p-12 text-12">{pretty(
                                quest.requirements,
                            )}</pre>
                    </section>
                </article>
            {:else if cinematic}
                <article class="space-y-84">
                    <div>
                        <Heading level={2}>{shortName(cinematic.name)}</Heading>
                        <p class="text-13 text-fg-muted"><Mono>{cinematic.name}</Mono> · {cinematic.bytes} bytes</p>
                    </div>
                    <section>
                        <Heading level={3}>Played by</Heading>
                        <ul class="mt-8 space-y-4">
                            {#each cinematic.used_by as use, index (index)}
                                <li>
                                    <button class="underline" onclick={() => void openQuest(use.quest)}>{use.quest}</button>
                                    <span class="text-13 text-fg-muted"
                                        >{use.class}
                                        {use.on_start ? "on accept" : "on complete"}{use.goal ? ` of goal ${use.goal}` : ""}</span
                                    >
                                </li>
                            {:else}
                                <li class="text-fg-muted">No quest result plays this cinematic yet.</li>
                            {/each}
                        </ul>
                    </section>
                    <section>
                        <Heading level={3}>Stages, actors and actions</Heading>
                        {#if cinematic.unread}<p class="text-state-wrong">{cinematic.unread}</p>{/if}
                        <pre class="mt-8 overflow-x-auto rounded-card border border-edge-quiet bg-surface-card p-12 text-12">{pretty(
                                cinematic.data,
                            )}</pre>
                    </section>
                </article>
            {:else}
                <p class="text-fg-muted">Choose a quest or a cinematic.</p>
            {/if}
        </main>
    </div>
</div>
