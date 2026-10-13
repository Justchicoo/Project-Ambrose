<!-- Project Ambrose by Imjustchico: The roles page: custom roles built from the permission catalog. Owners add, edit and remove roles; a role change rebuilds the resolution table and invalidates the sessions of affected holders immediately. -->
<script lang="ts">
    import { Button } from "$lib/components/ui/button/index.js";
    import { Input } from "$lib/components/ui/input/index.js";
    import { Label } from "$lib/components/ui/label/index.js";
    import { ApiError } from "$lib/api.svelte.js";
    import { panelRoles, createPanelRole, updatePanelRole, removePanelRole } from "$lib/supervision.svelte.js";
    import type { InferOutput } from "valibot";
    import { PanelRolesAnswer } from "$lib/schemas.js";
    import PageHeader from "../components/PageHeader.svelte";

    type Role = InferOutput<typeof PanelRolesAnswer>[number];
    let roles = $state<Role[]>([]);
    let failure = $state("");
    let notice = $state("");
    let busy = $state(false);
    let editing = $state<Role | null>(null);
    let newName = $state("");
    let newDescription = $state("");
    let newKeys = $state("");

    async function load() {
        try {
            roles = await panelRoles();
            failure = "";
        } catch (problem) {
            failure = problem instanceof ApiError ? problem.message : "The roles could not be read";
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
            await createPanelRole(
                newName,
                newDescription,
                newKeys
                    .split(",")
                    .map((key) => key.trim())
                    .filter(Boolean),
            );
            newName = "";
            newDescription = "";
            newKeys = "";
            notice = "Role created.";
            await load();
        } catch (problem) {
            failure = problem instanceof ApiError ? problem.message : "The role could not be created";
        } finally {
            busy = false;
        }
    }

    function startEdit(role: Role) {
        editing = { ...role, keys: [...role.keys] };
    }

    async function saveEdit() {
        if (!editing) return;
        busy = true;
        failure = "";
        try {
            await updatePanelRole(editing.id, {
                name: editing.name,
                description: editing.description,
                keys: editing.keys,
            });
            notice = `Role ${editing.name} updated; holders' permissions apply now.`;
            editing = null;
            await load();
        } catch (problem) {
            failure = problem instanceof ApiError ? problem.message : "The role could not be updated";
        } finally {
            busy = false;
        }
    }

    async function remove(role: Role) {
        if (role.shipped) return;
        if (!confirm(`Remove role ${role.name}?`)) return;
        busy = true;
        failure = "";
        try {
            await removePanelRole(role.id);
            notice = `Role ${role.name} removed.`;
            await load();
        } catch (problem) {
            failure = problem instanceof ApiError ? problem.message : "The role could not be removed";
        } finally {
            busy = false;
        }
    }
</script>

<PageHeader title="Roles" description="Custom roles built from the permission catalog." />

{#if failure}<p class="text-red-600">{failure}</p>{/if}
{#if notice}<p class="text-green-700">{notice}</p>{/if}

<h2 class="text-lg font-semibold mt-4">Roles</h2>
<ul>
    {#each roles as role (role.id)}
        <li>
            <strong>{role.name}</strong>{role.shipped ? " (shipped)" : ""}{role.ownerOnly ? " (owner-only)" : ""}
            — {role.keys.length} keys, {role.holders} holders
            <div class="flex gap-2">
                <Button onclick={() => startEdit(role)} disabled={busy}>Edit</Button>
                {#if !role.shipped}
                    <Button onclick={() => remove(role)} disabled={busy}>Remove</Button>
                {/if}
            </div>
        </li>
    {/each}
</ul>

{#if editing}
    <h2 class="text-lg font-semibold mt-4">Edit role</h2>
    <div>
        <Label for="edit-name">Name</Label>
        <Input id="edit-name" bind:value={editing.name} />
    </div>
    <div>
        <Label for="edit-description">Description</Label>
        <Input id="edit-description" bind:value={editing.description} />
    </div>
    <div>
        <Label for="edit-keys">Keys (comma separated)</Label>
        <Input
            id="edit-keys"
            value={editing.keys.join(", ")}
            oninput={(e) =>
                (editing!.keys = e.currentTarget.value
                    .split(",")
                    .map((k) => k.trim())
                    .filter(Boolean))}
        />
    </div>
    <div class="flex gap-2 mt-2">
        <Button onclick={saveEdit} disabled={busy}>Save</Button>
        <Button onclick={() => (editing = null)} disabled={busy}>Cancel</Button>
    </div>
{/if}

<h2 class="text-lg font-semibold mt-6">Create role</h2>
<div class="flex gap-2 items-end">
    <div>
        <Label for="new-role-name">Name</Label>
        <Input id="new-role-name" bind:value={newName} />
    </div>
    <div>
        <Label for="new-role-description">Description</Label>
        <Input id="new-role-description" bind:value={newDescription} />
    </div>
    <div>
        <Label for="new-role-keys">Keys (comma separated)</Label>
        <Input id="new-role-keys" bind:value={newKeys} />
    </div>
    <Button onclick={create} disabled={busy || !newName}>Create</Button>
</div>
