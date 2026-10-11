<!-- Project Ambrose by Imjustchico: Claiming a one-time invite: the invitation is read first so the invitee sees the scope, role and grants it carries and how long it lasts, then they choose the name and password they will sign in with, and when the panel's policy requires two-factor sign-in for what the invite grants, the enrollment page follows the claim. A link that is unknown, used, revoked or expired says only that. -->
<script lang="ts">
    import * as Card from "$lib/components/ui/card/index.js";
    import { Badge } from "$lib/components/ui/badge/index.js";
    import { Button } from "$lib/components/ui/button/index.js";
    import { Input } from "$lib/components/ui/input/index.js";
    import { Label } from "$lib/components/ui/label/index.js";
    import { ApiError, claimInvite, peekInvite } from "$lib/api.svelte.js";
    import type { InvitePeekAnswer } from "$lib/schemas.js";
    import { onMount } from "svelte";

    let { token, onDone }: { token: string; onDone: () => void } = $props();

    let invite = $state<InvitePeekAnswer | null>(null);
    let loading = $state(true);
    let invalid = $state(false);
    let username = $state("");
    let password = $state("");
    let busy = $state(false);
    let failure = $state("");
    let fields = $state<Record<string, string>>({});

    function describe(error: ApiError): string {
        if (error.code === "invite_not_found") return "That invite is unknown, used or revoked.";
        if (error.code === "invite_used") return "That invite has already been used.";
        if (error.code === "invite_expired") return "That invite has expired. Ask for a new one.";
        if (error.code === "invite_username_mismatch") return "That invite was made for a different username.";
        if (error.status === 429) return "Too many attempts. Wait a moment and try again.";
        if (error.status === 0) return "The panel could not reach its server. Check that it is still running.";
        return error.message;
    }

    async function load() {
        loading = true;
        invalid = false;
        try {
            invite = await peekInvite(token);
            if (invite.target_username) username = invite.target_username;
        } catch (problem) {
            invalid = true;
            failure = problem instanceof ApiError ? describe(problem) : "The invite could not be read.";
        } finally {
            loading = false;
        }
    }

    async function claim() {
        const missing: Record<string, string> = {};
        if (username.trim() === "") missing.username = "Choose the name you will sign in with.";
        if (password === "") missing.password = "Choose a password of at least twelve characters.";
        fields = missing;
        if (Object.keys(missing).length > 0) return;
        busy = true;
        failure = "";
        try {
            await claimInvite(token, username.trim(), password);
            password = "";
            onDone();
        } catch (problem) {
            if (problem instanceof ApiError) {
                failure = describe(problem);
                for (const [field, message] of Object.entries(problem.fields)) fields[field] = message;
            } else {
                failure = "The invite could not be claimed.";
            }
        } finally {
            busy = false;
        }
    }

    onMount(load);
</script>

<main class="flex min-h-svh items-center justify-center bg-background p-4">
    <Card.Root class="w-full max-w-md">
        <Card.Header>
            <Card.Title>You are invited to operate this panel</Card.Title>
            <Card.Description>Choose the name and password you will sign in with.</Card.Description>
        </Card.Header>
        <Card.Content>
            {#if loading}
                <p class="text-sm text-muted-foreground">Reading the invitation…</p>
            {:else if invalid || invite === null}
                <p class="text-sm text-destructive" role="alert">{failure}</p>
            {:else}
                <div class="mb-4 space-y-2 rounded-md border p-3 text-sm">
                    <div class="flex flex-wrap gap-2">
                        <Badge variant="outline">{invite.scope}</Badge>
                        {#if invite.role}<Badge>{invite.role}</Badge>{/if}
                    </div>
                    {#if invite.grants.length > 0}
                        <p><span class="text-muted-foreground">Grants:</span> <code class="text-xs">{invite.grants.join(", ")}</code></p>
                    {/if}
                    <p class="text-xs text-muted-foreground">
                        Expires {new Date(invite.expires_epoch_ms).toLocaleString()}.
                        {#if invite.two_factor_required}
                            This panel requires two-factor sign-in for what this invite grants; you will turn it on next.
                        {/if}
                    </p>
                </div>
                {#if failure !== ""}
                    <p class="mb-4 text-sm text-destructive" role="alert">{failure}</p>
                {/if}
                <form
                    class="space-y-4"
                    onsubmit={(event) => {
                        event.preventDefault();
                        void claim();
                    }}
                >
                    <div class="space-y-2">
                        <Label for="invite-username">Username</Label>
                        <Input
                            id="invite-username"
                            bind:value={username}
                            autocomplete="username"
                            disabled={invite.target_username !== null || busy}
                        />
                        {#if fields.username}<p class="text-xs text-destructive">{fields.username}</p>{/if}
                    </div>
                    <div class="space-y-2">
                        <Label for="invite-password">Password</Label>
                        <Input id="invite-password" type="password" bind:value={password} autocomplete="new-password" disabled={busy} />
                        {#if fields.password}<p class="text-xs text-destructive">{fields.password}</p>{/if}
                    </div>
                    <Button type="submit" class="w-full" disabled={busy}>{busy ? "Claiming…" : "Claim the invite"}</Button>
                </form>
            {/if}
        </Card.Content>
        <Card.Footer>
            <p class="text-xs text-muted-foreground">
                Already operate this panel?
                <a
                    class="underline"
                    href="#overview"
                    onclick={(event) => {
                        event.preventDefault();
                        onDone();
                    }}>Sign in</a
                >.
            </p>
        </Card.Footer>
    </Card.Root>
</main>
