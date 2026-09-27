<!-- Project Ambrose by Imjustchico: The reload page, live from each app: every store that can be rebuilt without stopping the server, the generation each is serving, how the last attempt went and every error it found, with a button to put one back in place or all of them in the order they depend on, shown only to a caller allowed to run a reload, so an operator changes what is live from here rather than from a terminal; while the app has not answered it says so rather than showing an empty list; and the changes no reload can take, which need a restart, each with why. -->
<script lang="ts">
    import * as Card from "$lib/components/ui/card/index.js";
    import * as Table from "$lib/components/ui/table/index.js";
    import { Button } from "$lib/components/ui/button/index.js";
    import { ApiError } from "$lib/api.svelte.js";
    import { may } from "$lib/permission.svelte.js";
    import { restartCases } from "$lib/restarts.js";
    import { candidates, reloadTargetsOf, runReload } from "$lib/supervision.svelte.js";
    import type { ReloadAnswer } from "$lib/schemas.js";
    import RefreshCwIcon from "@lucide/svelte/icons/refresh-cw";
    import PageHeader from "../components/PageHeader.svelte";
    import StatusBadge from "../components/StatusBadge.svelte";

    const choices = $derived(candidates());
    let chosen = $state("");
    let answer = $state<ReloadAnswer | null>(null);
    let failure = $state("");
    let running = $state("");
    let ran = $state(0);
    let answeredFor = "";

    const app = $derived(choices.includes(chosen) ? chosen : (choices[0] ?? ""));
    const canRun = $derived(may("reload.run", app));

    $effect(() => {
        const name = app;
        const attempt = ran;
        if (name === "") return;
        void attempt;
        if (name !== answeredFor) {
            answeredFor = name;
            answer = null;
            failure = "";
        }
        const controller = new AbortController();
        void (async () => {
            try {
                answer = await reloadTargetsOf(name, controller.signal);
                failure = "";
            } catch (problem) {
                if (controller.signal.aborted) return;
                answer = null;
                failure = problem instanceof ApiError ? problem.message : "What can be reloaded could not be read";
            }
        })();
        return () => controller.abort();
    });

    async function reload(target: string) {
        if (app === "" || running !== "") return;
        running = target;
        try {
            await runReload(app, target);
            failure = "";
        } catch (problem) {
            if (problem instanceof ApiError && problem.status === 409)
                failure = `${target === "all" ? "A target" : target} was not reloaded, and what was serving goes on serving; the list names every error it found`;
            else failure = problem instanceof ApiError ? problem.message : `${target} could not be reloaded`;
        } finally {
            running = "";
            ran += 1;
        }
    }

    const targets = $derived(answer?.targets ?? []);
</script>

<PageHeader title="Reload" description="What this app can rebuild without being stopped" />

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

<Card.Root>
    <Card.Header class="flex flex-row items-center justify-between gap-4">
        <div>
            <Card.Title>Targets</Card.Title>
            <Card.Description>In the order they are reloaded, so a target follows whatever it is built from.</Card.Description>
        </div>
        {#if canRun}
            <Button size="sm" disabled={running !== "" || targets.length === 0} onclick={() => reload("all")}>
                <RefreshCwIcon class="mr-2 size-4" />
                Reload all
            </Button>
        {/if}
    </Card.Header>
    <Card.Content>
        {#if answer === null && failure === ""}
            <p class="py-6 text-sm text-muted-foreground">Reading what {app === "" ? "this app" : app} can reload.</p>
        {:else if answer === null}
            <p class="py-6 text-sm text-muted-foreground">What can be reloaded is not known until {app} answers.</p>
        {:else if targets.length === 0}
            <p class="py-6 text-sm text-muted-foreground">This app has nothing registered that can be reloaded on its own.</p>
        {:else}
            <Table.Root>
                <Table.Header>
                    <Table.Row>
                        <Table.Head>Target</Table.Head>
                        <Table.Head>Generation</Table.Head>
                        <Table.Head>Last attempt</Table.Head>
                        {#if canRun}<Table.Head class="text-right">Reload</Table.Head>{/if}
                    </Table.Row>
                </Table.Header>
                <Table.Body>
                    {#each targets as target (target.target)}
                        <Table.Row>
                            <Table.Cell class="font-medium">{target.target}</Table.Cell>
                            <Table.Cell>{target.generation}</Table.Cell>
                            <Table.Cell class="whitespace-normal">
                                {#if !target.ran}
                                    <span class="text-sm text-muted-foreground">Not reloaded since this app started</span>
                                {:else if target.ok}
                                    <StatusBadge tone="healthy">Reloaded</StatusBadge>
                                {:else}
                                    <StatusBadge tone="wrong">Kept the last one</StatusBadge>
                                    <ul class="mt-2 list-disc pl-5 text-sm text-destructive">
                                        {#each target.errors as problem, index (index)}
                                            <li>{problem}</li>
                                        {/each}
                                    </ul>
                                {/if}
                            </Table.Cell>
                            {#if canRun}
                                <Table.Cell class="text-right">
                                    <Button
                                        variant="outline"
                                        size="sm"
                                        disabled={running !== ""}
                                        aria-label={`Reload ${target.target}`}
                                        onclick={() => reload(target.target)}
                                    >
                                        {running === target.target ? "Reloading" : "Reload"}
                                    </Button>
                                </Table.Cell>
                            {/if}
                        </Table.Row>
                    {/each}
                </Table.Body>
            </Table.Root>
        {/if}
    </Card.Content>
</Card.Root>

<Card.Root class="mt-4">
    <Card.Header>
        <Card.Title>Changes that need a restart</Card.Title>
        <Card.Description>
            No reload can take these, so the server has to be restarted for them. The options that take effect only at the next start are
            listed on the configuration page.
        </Card.Description>
    </Card.Header>
    <Card.Content class="space-y-4">
        <ul class="space-y-2 text-sm">
            {#each restartCases as restart (restart.change)}
                <li>
                    <span class="font-medium">{restart.change}</span>
                    <span class="text-muted-foreground">— {restart.reason}</span>
                </li>
            {/each}
        </ul>
        <Button variant="outline" size="sm" href="#config">Open the configuration page</Button>
    </Card.Content>
</Card.Root>
