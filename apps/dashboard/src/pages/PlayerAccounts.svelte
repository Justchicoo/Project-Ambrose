<!-- Project Ambrose by Imjustchico: Public player sign-up, email verification and password recovery on the panel listener, with non-enumerating request results and tokens sent only in POST bodies. -->
<script lang="ts">
    import * as Card from "$lib/components/ui/card/index.js";
    import { Button } from "$lib/components/ui/button/index.js";
    import { Input } from "$lib/components/ui/input/index.js";
    import { Label } from "$lib/components/ui/label/index.js";
    import {
        ApiError,
        playerRegistrationInfo,
        registerPlayer,
        requestPlayerPasswordReset,
        resetPlayerPassword,
        verifyPlayerEmail,
    } from "$lib/api.svelte.js";
    import type { InferOutput } from "valibot";
    import { PlayerRegistrationInfoAnswer } from "$lib/schemas.js";
    import { onMount } from "svelte";

    type Props = { page: string };
    type Fields = Record<string, string>;
    type RegistrationInfo = InferOutput<typeof PlayerRegistrationInfoAnswer>;

    let { page }: Props = $props();
    let username = $state("");
    let email = $state("");
    let password = $state("");
    let again = $state("");
    let captcha = $state("");
    let token = $state("");
    let info = $state<RegistrationInfo | null>(null);
    let busy = $state(false);
    let failure = $state("");
    let notice = $state("");
    let fields = $state<Fields>({});
    let captchaRequired = $state(false);

    const mode = $derived(page.startsWith("player/") ? page.slice(7) : page);
    const titles: Record<string, string> = {
        register: "Create a player account",
        recover: "Recover a player account",
        verify: "Verify your email",
        reset: "Choose a new password",
    };
    const title = $derived(titles[mode] ?? "Player account");

    $effect(() => {
        if (mode !== "verify" && mode !== "reset") return;
        const query = window.location.hash.split("?", 2)[1] ?? "";
        token = new URLSearchParams(query).get("token") ?? "";
    });

    onMount(() => {
        if (mode === "verify" || mode === "reset") return;
        void refreshInfo();
    });

    async function refreshInfo() {
        try {
            info = await playerRegistrationInfo();
            captchaRequired = info.captcha_required;
        } catch (problem) {
            failure = problem instanceof ApiError && problem.status === 404
                ? "Player registration and password recovery are turned off."
                : problem instanceof ApiError
                  ? problem.message
                  : "The player-account service could not be reached.";
        }
    }

    function describe(problem: unknown): string {
        if (problem instanceof ApiError) {
            if (problem.code === "captcha_required") {
                captchaRequired = true;
                void refreshInfo();
                return "Answer the captcha to continue.";
            }
            if (problem.status === 429) return "Too many requests. Wait a while, then try again.";
            if (problem.status === 0) return "The panel could not reach its server.";
            return problem.message;
        }
        return "The request could not be completed.";
    }

    async function submit(event: SubmitEvent) {
        event.preventDefault();
        failure = "";
        notice = "";
        fields = {};
        if (mode === "register" && (password === "" || password !== again)) {
            fields = password === "" ? { password: "Choose a password." } : { again: "Enter the same password again." };
            return;
        }
        busy = true;
        try {
            if (mode === "register") {
                await registerPlayer(username.trim(), email.trim(), password, captcha);
                notice = "If the request can be completed, check the email address for next steps.";
                password = "";
                again = "";
            } else if (mode === "recover") {
                await requestPlayerPasswordReset(email.trim(), captcha);
                notice = "If an account matches that address, a recovery link will be sent.";
            } else if (mode === "verify") {
                await verifyPlayerEmail(token);
                notice = "Your email is verified. You can now sign in to the game.";
            } else if (mode === "reset") {
                if (password !== again) {
                    fields = { again: "Enter the same password again." };
                    return;
                }
                await resetPlayerPassword(token, password);
                notice = "Your password has been changed. You can sign in to the game.";
                password = "";
                again = "";
            }
            captcha = "";
        } catch (problem) {
            failure = describe(problem);
        } finally {
            busy = false;
        }
    }
</script>

<main class="flex min-h-svh items-center justify-center bg-background p-4">
    <Card.Root class="w-full max-w-md shadow-xs">
        <Card.Header>
            <Card.Title class="font-serif text-2xl">{title}</Card.Title>
            <Card.Description>Player accounts are separate from panel operator accounts.</Card.Description>
        </Card.Header>
        <Card.Content>
            {#if failure}
                <p
                    class="mb-4 rounded-md border border-destructive/30 bg-destructive/5 p-3 text-sm text-destructive"
                    role="alert"
                >
                    {failure}
                </p>
            {/if}
            {#if notice}
                <p class="mb-4 rounded-md border border-healthy/30 bg-healthy/5 p-3 text-sm text-healthy" role="status">{notice}</p>
            {/if}
            {#if mode === "verify"}
                <form class="space-y-4" onsubmit={submit}>
                    <p class="text-sm text-muted-foreground">The verification token is kept in this page fragment and sent only in the request body.</p>
                    <Button class="w-full" disabled={busy || token === ""}>{busy ? "Verifying…" : "Verify email"}</Button>
                </form>
            {:else if mode === "register"}
                <form class="space-y-4" onsubmit={submit}>
                    <div class="space-y-2">
                        <Label for="player-username">Username</Label>
                        <Input id="player-username" autocomplete="username" bind:value={username} required />
                    </div>
                    <div class="space-y-2">
                        <Label for="player-email">Email address</Label>
                        <Input
                            id="player-email"
                            type="email"
                            autocomplete="email"
                            bind:value={email}
                            required={info?.email_verification ?? false}
                        />
                        {#if fields.email}<p class="text-sm text-destructive">{fields.email}</p>{/if}
                    </div>
                    <div class="space-y-2">
                        <Label for="player-password">Password</Label>
                        <Input id="player-password" type="password" autocomplete="new-password" bind:value={password} required />
                        {#if fields.password}<p class="text-sm text-destructive">{fields.password}</p>{/if}
                    </div>
                    <div class="space-y-2">
                        <Label for="player-password-again">Type the password again</Label>
                        <Input id="player-password-again" type="password" autocomplete="new-password" bind:value={again} required />
                        {#if fields.again}<p class="text-sm text-destructive">{fields.again}</p>{/if}
                    </div>
                    {#if captchaRequired}
                        <div class="space-y-2">
                            <Label for="player-captcha">Captcha response</Label>
                            <Input id="player-captcha" bind:value={captcha} required />
                            <p class="text-xs text-muted-foreground">Captcha provider: {info?.captcha_provider}. Complete its challenge and enter the response.</p>
                        </div>
                    {/if}
                    <Button class="w-full" disabled={busy || info === null}>{busy ? "Submitting…" : "Create account"}</Button>
                </form>
            {:else if mode === "recover"}
                <form class="space-y-4" onsubmit={submit}>
                    <div class="space-y-2">
                        <Label for="player-recovery-email">Email address</Label>
                        <Input id="player-recovery-email" type="email" autocomplete="email" bind:value={email} required />
                    </div>
                    {#if captchaRequired}
                        <div class="space-y-2">
                            <Label for="player-recovery-captcha">Captcha response</Label>
                            <Input id="player-recovery-captcha" bind:value={captcha} required />
                        </div>
                    {/if}
                    <Button class="w-full" disabled={busy || info === null}>{busy ? "Submitting…" : "Send recovery link"}</Button>
                </form>
            {:else if mode === "reset"}
                <form class="space-y-4" onsubmit={submit}>
                    <div class="space-y-2">
                        <Label for="player-reset-password">New password</Label>
                        <Input id="player-reset-password" type="password" autocomplete="new-password" bind:value={password} required />
                    </div>
                    <div class="space-y-2">
                        <Label for="player-reset-again">Type the password again</Label>
                        <Input id="player-reset-again" type="password" autocomplete="new-password" bind:value={again} required />
                        {#if fields.again}<p class="text-sm text-destructive">{fields.again}</p>{/if}
                    </div>
                    <Button class="w-full" disabled={busy || token === ""}>{busy ? "Changing…" : "Change password"}</Button>
                </form>
            {:else}
                <p class="text-sm text-muted-foreground">This player-account page was not found.</p>
            {/if}
        </Card.Content>
        <Card.Footer class="flex justify-between text-sm">
            <a class="text-primary underline underline-offset-4" href="#overview">Back to sign in</a>
            {#if mode === "register"}
                <a class="text-primary underline underline-offset-4" href="#player/recover">Forgot password?</a>
            {:else if mode === "recover"}
                <a class="text-primary underline underline-offset-4" href="#player/register">Create account</a>
            {/if}
        </Card.Footer>
    </Card.Root>
</main>
