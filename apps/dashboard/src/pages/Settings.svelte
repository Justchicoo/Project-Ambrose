<!-- Project Ambrose by Imjustchico: The config page, live from each app: every option the app has loaded with the value in use, the value its .conf.dist ships, the layer, file and line it was read from, secrets shown only as the mask, and the reason beside each option that only takes effect at the app's next start, searchable and grouped by the first word of the key. -->
<script lang="ts">
    import * as Card from "$lib/components/ui/card/index.js";
    import * as Select from "$lib/components/ui/select/index.js";
    import * as Table from "$lib/components/ui/table/index.js";
    import { Input } from "$lib/components/ui/input/index.js";
    import { ApiError } from "$lib/api.svelte.js";
    import { live } from "$lib/status.svelte.js";
    import { servedBy, settingsOf, supervised, supervisorServes } from "$lib/supervision.svelte.js";
    import type { SettingsAnswer } from "$lib/schemas.js";
    import SearchIcon from "@lucide/svelte/icons/search";
    import PageHeader from "../components/PageHeader.svelte";
    import StatusBadge from "../components/StatusBadge.svelte";

    const layers: Record<string, string> = {
        default: "Shipped default",
        module_default: "Module default",
        config: "This app's .conf",
        module_config: "A conf.d file",
        environment: "Environment variable",
        override: "Command line",
    };

    const choices = $derived(supervisorServes() ? [servedBy(), ...supervised().map((app) => app.name)] : [servedBy()]);
    let chosen = $state("");
    let answer = $state<SettingsAnswer | null>(null);
    let failure = $state("");
    let search = $state("");

    const app = $derived(choices.includes(chosen) ? chosen : (choices[0] ?? ""));

    $effect(() => {
        const name = app;
        if (name === "") return;
        const controller = new AbortController();
        void (async () => {
            try {
                answer = await settingsOf(name, controller.signal);
                failure = "";
            } catch (problem) {
                if (controller.signal.aborted) return;
                answer = null;
                failure = problem instanceof ApiError ? problem.message : "The settings could not be read";
            }
        })();
        return () => controller.abort();
    });

    const shown = $derived.by(() => {
        const text = search.trim().toLowerCase();
        const settings = (answer?.settings ?? []).filter(
            (setting) => text === "" || setting.key.toLowerCase().includes(text) || setting.value.toLowerCase().includes(text),
        );
        const groups: { name: string; list: typeof settings }[] = [];
        for (const setting of settings) {
            const dot = setting.key.indexOf(".");
            const name = dot === -1 ? "General" : setting.key.slice(0, dot);
            const held = groups.find((group) => group.name === name);
            if (held) held.list.push(setting);
            else groups.push({ name, list: [setting] });
        }
        return groups;
    });

    const counted = $derived(answer?.settings.length ?? 0);
    const restarts = $derived((answer?.settings ?? []).filter((setting) => setting.restart_reason !== null).length);
</script>

<PageHeader
    title="Settings"
    description={`Every option ${app === "" ? "this app" : app} has loaded, with where each value came from. Editing them from here arrives with milestone 17.13.`}
>
    {#snippet actions()}
        {#if choices.length > 1}
            <Select.Root type="single" bind:value={chosen}>
                <Select.Trigger class="w-44" aria-label="App whose settings are shown">{app}</Select.Trigger>
                <Select.Content>
                    {#each choices as name (name)}
                        <Select.Item value={name}>{name}</Select.Item>
                    {/each}
                </Select.Content>
            </Select.Root>
        {/if}
        <div class="relative">
            <SearchIcon class="absolute top-2.5 left-2.5 size-4 text-muted-foreground" />
            <Input class="w-56 pl-8" placeholder="Search options" bind:value={search} aria-label="Search options" />
        </div>
    {/snippet}
</PageHeader>

{#if failure}
    <Card.Root class="shadow-xs">
        <Card.Header>
            <Card.Title>The settings could not be read</Card.Title>
            <Card.Description>{failure}</Card.Description>
        </Card.Header>
    </Card.Root>
{:else if !answer}
    <p class="text-sm text-muted-foreground">Reading the settings of {app === "" ? "this app" : app}.</p>
{:else}
    <div class="flex flex-wrap items-center gap-2 text-sm text-muted-foreground">
        <span>{counted} option{counted === 1 ? "" : "s"} from <span class="font-mono text-xs">{answer.file}</span></span>
        {#if restarts > 0}
            <StatusBadge tone="waiting">{restarts} need a restart</StatusBadge>
        {/if}
        {#if live.status && live.status.app !== app}
            <span>Read through the supervisor.</span>
        {/if}
    </div>

    {#each shown as group (group.name)}
        <Card.Root class="py-0 shadow-xs">
            <Table.Root>
                <Table.Header>
                    <Table.Row class="hover:bg-transparent">
                        <Table.Head class="pl-6">{group.name}</Table.Head>
                        <Table.Head>Value in use</Table.Head>
                        <Table.Head class="hidden lg:table-cell">Shipped default</Table.Head>
                        <Table.Head class="hidden md:table-cell">Read from</Table.Head>
                    </Table.Row>
                </Table.Header>
                <Table.Body>
                    {#each group.list as setting (setting.key)}
                        <Table.Row>
                            <Table.Cell class="pl-6 align-top">
                                <div class="font-mono text-xs font-medium">{setting.key}</div>
                                <div class="mt-1 flex flex-wrap gap-1">
                                    {#if setting.secret}<StatusBadge tone="mine">Secret</StatusBadge>{/if}
                                    {#if setting.restart_reason}<StatusBadge tone="waiting">Restart required</StatusBadge>{/if}
                                </div>
                                {#if setting.restart_reason}
                                    <div class="mt-1 max-w-80 text-xs text-muted-foreground">{setting.restart_reason}</div>
                                {/if}
                            </Table.Cell>
                            <Table.Cell class="align-top font-mono text-xs break-all">
                                {setting.value === "" ? "—" : setting.value}
                                {#if setting.default !== null && setting.default !== setting.value}
                                    <div class="mt-1 font-sans text-xs text-waiting">Changed from the default</div>
                                {/if}
                            </Table.Cell>
                            <Table.Cell class="hidden align-top font-mono text-xs break-all lg:table-cell"
                                >{setting.default === null ? "Not shipped" : setting.default === "" ? "—" : setting.default}</Table.Cell
                            >
                            <Table.Cell class="hidden align-top text-xs md:table-cell">
                                <div>{layers[setting.layer] ?? setting.layer}</div>
                                <div class="text-muted-foreground">
                                    {setting.file}{setting.line > 0 ? `:${setting.line}` : ""}
                                </div>
                            </Table.Cell>
                        </Table.Row>
                    {/each}
                </Table.Body>
            </Table.Root>
        </Card.Root>
    {/each}
    {#if shown.length === 0}
        <p class="text-sm text-muted-foreground">No option matches “{search}”.</p>
    {/if}
{/if}
