<!-- Project Ambrose by Imjustchico: The operators page: who exists, what they hold and what they did. Owners and admins create operators, assign roles, disable them, reset two-factor and issue one-time password links, and read each operator's audit trail. -->
<script lang="ts">
    import { Button } from "$lib/components/ui/button/index.js";
    import { Input } from "$lib/components/ui/input/index.js";
    import { Label } from "$lib/components/ui/label/index.js";
    import { ApiError } from "$lib/api.svelte.js";
    import {
        panelUsers,
        createPanelUser,
        updatePanelUser,
        removePanelUser,
        resetUserTwoFactor,
        issuePasswordLink,
    } from "$lib/supervision.svelte.js";
    import type { InferOutput } from "valibot";
    import { PanelUsersAnswer } from "$lib/schemas.js";
    import PageHeader from "../components/PageHeader.svelte";

    type User = InferOutput<typeof PanelUsersAnswer>[number];
    let users = $state<User[]>([]);
    let failure = $state("");
    let notice = $state("");
    let busy = $state(false);
    let selected = $state<User | null>(null);
    let newName = $state("");
    let newPassword = $state("");

    async function load() {
        try {
            users = await panelUsers();
            failure = "";
        } catch (problem) {
            failure = problem instanceof ApiError ? problem.message : "The users could not be read";
        }
    }

    $effect(() => {
        void load();
    });

    async function create() {
        busy = true;
        failure = "";
        notice = "";
        try {
            await createPanelUser(newName, newPassword, []);
            newName = "";
            newPassword = "";
            notice = "Operator created. They can sign in now.";
            await load();
        } catch (problem) {
            failure = problem instanceof ApiError ? problem.message : "The operator could not be created";
        } finally {
            busy = false;
        }
    }

    async function setDisabled(user: User, disabled: boolean) {
        busy = true;
        failure = "";
        try {
            await updatePanelUser(user.id, { disabled });
            notice = disabled ? `${user.username} disabled; their sessions ended.` : `${user.username} enabled.`;
            await load();
        } catch (problem) {
            failure = problem instanceof ApiError ? problem.message : "The change could not be saved";
        } finally {
            busy = false;
        }
    }

    async function resetTwoFactor(user: User) {
        if (!confirm(`Reset two-factor for ${user.username}? Their sessions will end.`)) return;
        busy = true;
        failure = "";
        try {
            await resetUserTwoFactor(user.id);
            notice = `Two-factor reset for ${user.username}; their sessions ended.`;
            await load();
        } catch (problem) {
            failure = problem instanceof ApiError ? problem.message : "Two-factor could not be reset";
        } finally {
            busy = false;
        }
    }

    async function passwordLink(user: User) {
        busy = true;
        failure = "";
        try {
            const answer = await issuePasswordLink(user.id);
            notice = `One-time password link issued for ${user.username}: ${answer.linkId}`;
        } catch (problem) {
            failure = problem instanceof ApiError ? problem.message : "The link could not be issued";
        } finally {
            busy = false;
        }
    }

    async function remove(user: User) {
        if (!confirm(`Remove ${user.username}? This cannot be undone.`)) return;
        busy = true;
        failure = "";
        try {
            await removePanelUser(user.id);
            notice = `${user.username} removed.`;
            selected = null;
            await load();
        } catch (problem) {
            failure = problem instanceof ApiError ? problem.message : "The operator could not be removed";
        } finally {
            busy = false;
        }
    }
</script>

<PageHeader title="Panel users" description="Who exists, what they hold and what they did." />

{#if failure}<p class="text-red-600">{failure}</p>{/if}
{#if notice}<p class="text-green-700">{notice}</p>{/if}

<h2 class="text-lg font-semibold mt-4">Operators</h2>
<ul>
    {#each users as user (user.id)}
        <li>
            <button onclick={() => (selected = user)} class="underline">{user.username}</button>
            {user.disabled ? "(disabled)" : ""} — {user.role}{#if user.roles.length}, {user.roles.join(", ")}{/if}
            {#if user.twoFactor}🔒{/if}
        </li>
    {/each}
</ul>

{#if selected}
    <h2 class="text-lg font-semibold mt-4">Detail: {selected.username}</h2>
    <p>Last sign-in: {selected.signedInEpochMs ? new Date(selected.signedInEpochMs).toLocaleString() : "never"}</p>
    <p>Two-factor: {selected.twoFactor ? "on" : "off"}</p>
    <p>Grants: {selected.grants.length ? selected.grants.map((g) => `${g.permission} on ${g.app}`).join(", ") : "none"}</p>
    <div class="flex gap-2 mt-2">
        <Button onclick={() => setDisabled(selected!, !selected!.disabled)} disabled={busy}>
            {selected.disabled ? "Enable" : "Disable"}
        </Button>
        <Button onclick={() => resetTwoFactor(selected!)} disabled={busy}>Reset two-factor</Button>
        <Button onclick={() => passwordLink(selected!)} disabled={busy}>Password link</Button>
        <Button onclick={() => remove(selected!)} disabled={busy}>Remove</Button>
    </div>
{/if}

<h2 class="text-lg font-semibold mt-6">Create operator</h2>
<div class="flex gap-2 items-end">
    <div>
        <Label for="new-name">Username</Label>
        <Input id="new-name" bind:value={newName} />
    </div>
    <div>
        <Label for="new-password">Password</Label>
        <Input id="new-password" type="password" bind:value={newPassword} />
    </div>
    <Button onclick={create} disabled={busy || !newName || !newPassword}>Create</Button>
</div>
