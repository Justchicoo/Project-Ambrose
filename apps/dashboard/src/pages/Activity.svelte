<!-- Project Ambrose by Imjustchico: Who did what, when and from where: the panel's activity log for everything, for the signed-in operator's own actions, for one operator by id, or for one app, where the app's own command record follows the panel's, each scope offered only to an operator the server would answer for it. -->
<script lang="ts">
    import { Input } from "$lib/components/ui/input/index.js";
    import { Label } from "$lib/components/ui/label/index.js";
    import { may } from "$lib/permission.svelte.js";
    import { candidates, type ActivityScope } from "$lib/supervision.svelte.js";
    import PageHeader from "../components/PageHeader.svelte";
    import ActivityLog from "../components/ActivityLog.svelte";
    import AppCommandRecord from "../components/AppCommandRecord.svelte";

    const apps = $derived(candidates().filter((app) => may("activity.read", app)));
    const everything = $derived(may("activity.read"));
    let chosen = $state(may("activity.read") ? "all" : "me");
    let user = $state("");

    const choice = $derived(chosen === "all" && !everything ? "me" : chosen);
    const scope = $derived<ActivityScope>(
        choice === "all" ? { kind: "all" } : choice === "me" ? { kind: "me" } : { kind: "app", app: choice.slice("app:".length) },
    );
</script>

<PageHeader title="Activity" description="Who did what, when and from where, refused attempts included." />

<div class="mb-4 flex flex-wrap items-end gap-4">
    <div class="grid gap-1">
        <Label for="activity-scope">Show</Label>
        <select id="activity-scope" bind:value={chosen} class="h-9 rounded-md border bg-background px-2 text-sm">
            {#if everything}
                <option value="all">Everything</option>
            {/if}
            <option value="me">My activity</option>
            {#each apps as app (app)}
                <option value={`app:${app}`}>{app}</option>
            {/each}
        </select>
    </div>
    {#if scope.kind === "all"}
        <div class="grid gap-1">
            <Label for="activity-user">Operator id</Label>
            <Input id="activity-user" class="w-32" inputmode="numeric" bind:value={user} placeholder="Anyone" />
        </div>
    {/if}
</div>

<ActivityLog {scope} user={scope.kind === "all" ? user : ""} />

{#if scope.kind === "app"}
    <div class="mt-4">
        <AppCommandRecord app={scope.app} />
    </div>
{/if}
