<!-- Project Ambrose by Imjustchico: Inviting operators: the open, used, revoked and expired one-time links with what each carries and a revoke for the live ones, and the form that makes a new one — the scope its grants apply at, the role, an optional name it is made for, how long it lasts and a note — with the grant editor for the extra grants and a review before the link is made. An invite naming an operator that already exists adds the grant at once instead of making a link. -->
<script lang="ts">
    import * as Card from "$lib/components/ui/card/index.js";
    import * as Table from "$lib/components/ui/table/index.js";
    import { Badge } from "$lib/components/ui/badge/index.js";
    import { Button } from "$lib/components/ui/button/index.js";
    import { Input } from "$lib/components/ui/input/index.js";
    import { Label } from "$lib/components/ui/label/index.js";
    import * as Select from "$lib/components/ui/select/index.js";
    import { Textarea } from "$lib/components/ui/textarea/index.js";
    import { ApiError, createInvite, listInvites, permissionCatalog, revokeInvite, session, type InviteDraft } from "$lib/api.svelte.js";
    import type { InviteRow } from "$lib/schemas.js";
    import GrantEditor from "../components/GrantEditor.svelte";
    import PageHeader from "../components/PageHeader.svelte";
    import { onMount } from "svelte";
    import { SvelteSet } from "svelte/reactivity";

    let invites = $state<InviteRow[]>([]);
    let roles = $state<string[]>([]);
    let loading = $state(true);
    let busy = $state(false);
    let failure = $state("");
    let created = $state<{ link: string; applied: boolean; username: string } | null>(null);

    let scope = $state("gameserver");
    let role = $state("");
    let targetUsername = $state("");
    let ttlHours = $state(72);
    let note = $state("");
    let grants = $state<string[]>([]);

    const grantable = $derived.by(() => {
        const keys = new SvelteSet(session.user?.permissions ?? []);
        for (const key of session.user?.grants?.[scope] ?? []) keys.add(key);
        return keys;
    });
    const isOwner = $derived(session.user?.owner === true || session.user?.role === "owner");

    function when(epochMs: number): string {
        const date = new Date(epochMs);
        return Number.isNaN(date.getTime()) ? "Unknown time" : date.toLocaleString();
    }

    function stateBadge(state: InviteRow["state"]): "default" | "secondary" | "destructive" | "outline" {
        if (state === "live") return "default";
        if (state === "expired") return "destructive";
        return "secondary";
    }

    async function refresh() {
        loading = true;
        failure = "";
        try {
            invites = (await listInvites()).invites;
            if (roles.length === 0) roles = (await permissionCatalog()).roles.map((entry) => entry.name);
        } catch (problem) {
            failure = problem instanceof ApiError ? problem.message : "The invites could not be read.";
        } finally {
            loading = false;
        }
    }

    async function makeInvite(keys: string[]) {
        busy = true;
        failure = "";
        created = null;
        try {
            const draft: InviteDraft = { scope: scope.trim(), grants: keys };
            if (role !== "") draft.role = role;
            if (targetUsername.trim() !== "") draft.target_username = targetUsername.trim();
            if (ttlHours !== 72) draft.ttl_hours = ttlHours;
            if (note.trim() !== "") draft.note = note.trim();
            const answer = await createInvite(draft);
            if (answer.applied) {
                created = { link: "", applied: true, username: answer.username ?? targetUsername.trim() };
            } else {
                const token = answer.token ?? "";
                created = {
                    link: `${window.location.origin}${window.location.pathname}#invite?token=${encodeURIComponent(token)}`,
                    applied: false,
                    username: "",
                };
            }
            grants = [];
            await refresh();
        } catch (problem) {
            failure = problem instanceof ApiError ? problem.message : "The invite could not be made.";
        } finally {
            busy = false;
        }
    }

    async function revoke(id: number) {
        busy = true;
        failure = "";
        try {
            await revokeInvite(id);
            await refresh();
        } catch (problem) {
            failure = problem instanceof ApiError ? problem.message : "The invite could not be revoked.";
        } finally {
            busy = false;
        }
    }

    async function copyLink(link: string) {
        try {
            await navigator.clipboard.writeText(link);
        } catch {
            failure = "The link could not be copied; select it by hand.";
        }
    }

    onMount(refresh);
</script>

<div class="space-y-6">
    <PageHeader title="Invites" description="One-time links that bring new operators in with exactly the rights you hold, never more." />

    {#if failure !== ""}
        <p class="text-sm text-destructive">{failure}</p>
    {/if}

    {#if created !== null}
        <Card.Root>
            <Card.Header>
                <Card.Title>{created.applied ? "Grant added" : "Invite ready"}</Card.Title>
            </Card.Header>
            <Card.Content>
                {#if created.applied}
                    <p class="text-sm">
                        {created.username} already operates this panel, so the grant was added at once and they were notified in the panel.
                    </p>
                {:else}
                    <p class="text-sm text-muted-foreground">Send this link within {ttlHours} hours. It works once.</p>
                    <div class="mt-2 flex gap-2">
                        <Input readonly value={created.link} class="font-mono text-xs" />
                        <Button variant="outline" onclick={() => copyLink(created.link)}>Copy</Button>
                    </div>
                {/if}
            </Card.Content>
        </Card.Root>
    {/if}

    <Card.Root>
        <Card.Header>
            <Card.Title>Invite an operator</Card.Title>
            <Card.Description>Naming an operator that already exists adds the grant at once instead of making a link.</Card.Description>
        </Card.Header>
        <Card.Content class="space-y-4">
            <div class="grid gap-4 sm:grid-cols-2">
                <div class="space-y-2">
                    <Label for="invite-scope">Scope</Label>
                    <Input id="invite-scope" bind:value={scope} placeholder="gameserver" />
                    <p class="text-xs text-muted-foreground">The app the extra grants apply at.</p>
                </div>
                <div class="space-y-2">
                    <Label for="invite-role">Role</Label>
                    <Select.Root type="single" bind:value={role}>
                        <Select.Trigger id="invite-role"><span>{role === "" ? "No role change" : role}</span></Select.Trigger>
                        <Select.Content>
                            <Select.Item value="">No role change</Select.Item>
                            {#each roles as name (name)}
                                <Select.Item value={name}>{name}</Select.Item>
                            {/each}
                        </Select.Content>
                    </Select.Root>
                </div>
                <div class="space-y-2">
                    <Label for="invite-target">For an existing operator</Label>
                    <Input id="invite-target" bind:value={targetUsername} placeholder="username" autocomplete="off" />
                </div>
                <div class="space-y-2">
                    <Label for="invite-ttl">Lasts, in hours</Label>
                    <Input id="invite-ttl" type="number" min={1} max={720} bind:value={ttlHours} />
                </div>
            </div>
            <div class="space-y-2">
                <Label for="invite-note">Note</Label>
                <Textarea id="invite-note" bind:value={note} rows={2} placeholder="Who this is for" />
            </div>
            <GrantEditor {scope} {grantable} {isOwner} bind:selected={grants} confirmLabel="Review invite" onConfirm={makeInvite} />
        </Card.Content>
    </Card.Root>

    <Card.Root>
        <Card.Header>
            <Card.Title>Invitations</Card.Title>
        </Card.Header>
        <Card.Content>
            {#if loading}
                <p class="text-sm text-muted-foreground">Reading the invites…</p>
            {:else if invites.length === 0}
                <p class="text-sm text-muted-foreground">No invites yet.</p>
            {:else}
                <Table.Root>
                    <Table.Header>
                        <Table.Row>
                            <Table.Head>Scope</Table.Head>
                            <Table.Head>Role</Table.Head>
                            <Table.Head>Grants</Table.Head>
                            <Table.Head>For</Table.Head>
                            <Table.Head>Expires</Table.Head>
                            <Table.Head>State</Table.Head>
                            <Table.Head><span class="sr-only">Revoke</span></Table.Head>
                        </Table.Row>
                    </Table.Header>
                    <Table.Body>
                        {#each invites as invite (invite.id)}
                            <Table.Row>
                                <Table.Cell><code class="text-xs">{invite.scope}</code></Table.Cell>
                                <Table.Cell>{invite.role ?? "—"}</Table.Cell>
                                <Table.Cell>
                                    {#if invite.grants.length === 0}
                                        <span class="text-muted-foreground">—</span>
                                    {:else}
                                        <span class="text-xs">{invite.grants.join(", ")}</span>
                                    {/if}
                                </Table.Cell>
                                <Table.Cell>{invite.target_username ?? "anyone"}</Table.Cell>
                                <Table.Cell class="text-xs">{when(invite.expires_epoch_ms)}</Table.Cell>
                                <Table.Cell><Badge variant={stateBadge(invite.state)}>{invite.state}</Badge></Table.Cell>
                                <Table.Cell class="text-right">
                                    {#if invite.state === "live"}
                                        <Button variant="ghost" size="sm" disabled={busy} onclick={() => revoke(invite.id)}>Revoke</Button>
                                    {/if}
                                </Table.Cell>
                            </Table.Row>
                        {/each}
                    </Table.Body>
                </Table.Root>
            {/if}
        </Card.Content>
    </Card.Root>
</div>
