<!-- Project Ambrose by Imjustchico: The page a signed-out browser sees, which asks for what the host it came from signs people in with: a panel with no operator yet asks for the token from the one-time link the supervisor printed and the name and password to make the owner with, a panel that has one asks for a name and password, and an app's own listener asks for its admin token, which is traded once for a session so nothing is kept in the browser; a field's own problem sits beside it, any other refusal sits above the form with its request id, and a session that has just ended says so. -->
<script lang="ts">
    import * as Card from "$lib/components/ui/card/index.js";
    import { Button } from "$lib/components/ui/button/index.js";
    import { Input } from "$lib/components/ui/input/index.js";
    import { Label } from "$lib/components/ui/label/index.js";
    import { ApiError, claimOwner, session, signIn, signInAsUser } from "$lib/api.svelte.js";
    import CircleAlertIcon from "@lucide/svelte/icons/circle-alert";
    import LogInIcon from "@lucide/svelte/icons/log-in";

    let token = $state("");
    let username = $state("");
    let password = $state("");
    let busy = $state(false);
    let fields = $state<Fields>({});
    let problem = $state<ApiError | null>(null);
    let requestId = $state("");

    type Fields = Record<string, string>;

    const claiming = $derived(session.panel && session.needsOwner);
    const asUser = $derived(session.panel && !session.needsOwner);
    const mine = new Set(["token", "username", "password"]);
    const others = $derived(Object.entries(fields).filter(([field]) => !mine.has(field)));

    const linkFromTheStart = new URLSearchParams(window.location.hash.split("?")[1] ?? "").get("token") ?? "";
    if (linkFromTheStart !== "") token = linkFromTheStart;

    function describe(error: ApiError): string {
        if (error.code === "wrong_token") return "That is not this app's admin token.";
        if (error.code === "sign_in_refused") return "That name and password do not sign in.";
        if (error.code === "not_this_machine") return "The first operator is made from the machine the supervisor runs on.";
        if (error.code === "link_refused") return "That is not the token the supervisor printed.";
        if (error.code === "link_expired") return "That link has been used or has run out. Restart the supervisor for another.";
        if (error.code === "already_claimed") return "This panel already has an operator. Sign in with a name and password.";
        if (error.status === 429) return "Too many attempts. Wait a moment and try again.";
        if (error.status === 403)
            return "The server refused a sign-in from this page's address. Open the panel from the address it prints.";
        if (error.status === 0) return "The panel could not reach its server. Check that it is still running.";
        return error.message;
    }

    function required(): Fields {
        const missing: Fields = {};
        if (claiming && token.trim() === "") missing.token = "Paste the token from the link the supervisor printed.";
        if (asUser || claiming) {
            if (username.trim() === "") missing.username = claiming ? "Choose the name you will sign in with." : "Enter your name.";
            if (password === "") missing.password = claiming ? "Choose a password of at least twelve characters." : "Enter your password.";
        } else if (token.trim() === "") {
            missing.token = "Enter this app's admin token.";
        }
        return missing;
    }

    async function submit(event: SubmitEvent) {
        event.preventDefault();
        problem = null;
        requestId = "";
        fields = required();
        if (Object.keys(fields).length > 0) return;
        busy = true;
        try {
            if (claiming) await claimOwner(token.trim(), username.trim(), password);
            else if (asUser) await signInAsUser(username.trim(), password);
            else await signIn(token.trim());
            token = "";
            password = "";
        } catch (failure) {
            if (failure instanceof ApiError) {
                fields = failure.fields;
                problem = Object.keys(failure.fields).length === 0 ? failure : null;
                requestId = failure.requestId;
            } else {
                problem = new ApiError(0, "failed", "Signing in failed for a reason the panel could not read.", "");
            }
        } finally {
            busy = false;
        }
    }
</script>

<main class="flex min-h-svh items-center justify-center bg-background p-4">
    <Card.Root class="w-full max-w-sm shadow-xs">
        <Card.Header>
            <div
                class="mb-2 flex size-10 items-center justify-center rounded-lg bg-primary font-serif text-xl font-bold text-primary-foreground"
            >
                A
            </div>
            <Card.Title class="font-serif text-2xl">
                <h1>{claiming ? "Make the first operator" : "Sign in to Ambrose"}</h1>
            </Card.Title>
            <Card.Description>
                {#if claiming}
                    This panel has no operator yet. Paste the token from the link the supervisor printed and choose the name and password
                    you will sign in with.
                {:else if asUser}
                    Sign in with your panel account.
                {:else}
                    Enter the admin token this app keeps. It is traded once for a session in this browser and is never stored here.
                {/if}
            </Card.Description>
        </Card.Header>
        <Card.Content>
            <form class="space-y-4" onsubmit={submit} novalidate>
                {#if session.ended}
                    <p class="rounded-md border px-3 py-2 text-sm text-muted-foreground" role="status">
                        Your session ended. Sign in again to carry on.
                    </p>
                {/if}
                {#if problem || others.length > 0}
                    <div class="flex gap-2 rounded-md border border-destructive/30 bg-destructive/5 px-3 py-2 text-sm" role="alert">
                        <CircleAlertIcon class="mt-0.5 size-4 shrink-0 text-destructive" />
                        <div class="space-y-1">
                            {#if problem}<p>{describe(problem)}</p>{/if}
                            {#each others as [field, text] (field)}
                                <p><span class="font-mono">{field}</span>: {text}</p>
                            {/each}
                            {#if requestId}
                                <p class="text-xs text-muted-foreground">Request <span class="font-mono select-all">{requestId}</span></p>
                            {/if}
                        </div>
                    </div>
                {:else if requestId}
                    <p class="text-xs text-muted-foreground">Request <span class="font-mono select-all">{requestId}</span></p>
                {/if}
                {#if claiming || !asUser}
                    <div class="space-y-2">
                        <Label for="sign-in-token">{claiming ? "Link token" : "Admin token"}</Label>
                        <Input
                            id="sign-in-token"
                            type="password"
                            autocomplete="off"
                            spellcheck="false"
                            bind:value={token}
                            aria-invalid={fields.token !== undefined}
                            aria-describedby={fields.token !== undefined ? "sign-in-token-problem" : undefined}
                            class="font-mono"
                        />
                        {#if fields.token}<p id="sign-in-token-problem" class="text-sm text-destructive">{fields.token}</p>{/if}
                    </div>
                {/if}
                {#if claiming || asUser}
                    <div class="space-y-2">
                        <Label for="sign-in-username">Name</Label>
                        <Input
                            id="sign-in-username"
                            autocomplete="username"
                            spellcheck="false"
                            bind:value={username}
                            aria-invalid={fields.username !== undefined}
                            aria-describedby={fields.username !== undefined ? "sign-in-username-problem" : undefined}
                        />
                        {#if fields.username}<p id="sign-in-username-problem" class="text-sm text-destructive">{fields.username}</p>{/if}
                    </div>
                    <div class="space-y-2">
                        <Label for="sign-in-password">Password</Label>
                        <Input
                            id="sign-in-password"
                            type="password"
                            autocomplete={claiming ? "new-password" : "current-password"}
                            bind:value={password}
                            aria-invalid={fields.password !== undefined}
                            aria-describedby={fields.password !== undefined ? "sign-in-password-problem" : undefined}
                        />
                        {#if fields.password}<p id="sign-in-password-problem" class="text-sm text-destructive">{fields.password}</p>{/if}
                    </div>
                {/if}
                <Button type="submit" class="w-full" disabled={busy}>
                    <LogInIcon />{busy ? "Signing in" : claiming ? "Make me the owner" : "Sign in"}
                </Button>
            </form>
        </Card.Content>
        <Card.Footer>
            <p class="text-xs text-muted-foreground">
                {#if claiming}
                    The link is printed once, to the supervisor's console and log, and works only from this machine.
                {:else if asUser}
                    Lost your password? An operator can issue a reset from the supervisor's console with
                    <span class="font-mono">panel user reset-password</span>.
                {:else}
                    The token is <span class="font-mono">Admin.Token</span> in the app's config, or the token file the app names in its log when
                    it generates one.
                {/if}
            </p>
        </Card.Footer>
    </Card.Root>
</main>
