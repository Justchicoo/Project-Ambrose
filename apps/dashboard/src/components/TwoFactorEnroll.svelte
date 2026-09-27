<!-- Project Ambrose by Imjustchico: Turning two-factor sign-in on: the panel's setup secret written in groups of four to type into an authenticator app, with the otpauth link that opens one on the same device, then the operator's password and a current code from the app, and when the operator is moving from an app they already use, a current code or one recovery code from that app too, so a session and a password alone cannot swap the factor out, after which the ten recovery codes are shown once in a dialog and only when that dialog closes is the operator's new state adopted, so a page that goes away once two-factor is on cannot take the codes with it. A secret already set up is shown again rather than swapped, unless the operator is moving to another app. -->
<script lang="ts">
    import * as Card from "$lib/components/ui/card/index.js";
    import { Button } from "$lib/components/ui/button/index.js";
    import { Input } from "$lib/components/ui/input/index.js";
    import { Label } from "$lib/components/ui/label/index.js";
    import { ApiError, adoptUser, setUpTwoFactor, turnOnTwoFactor, type SecondFactor } from "$lib/api.svelte.js";
    import { groupSecret, readCode, readRecoveryCode } from "$lib/twofactor.js";
    import type { PanelUser, TwoFactorSetup } from "$lib/schemas.js";
    import ShieldCheckIcon from "@lucide/svelte/icons/shield-check";
    import SmartphoneIcon from "@lucide/svelte/icons/smartphone";
    import { onMount } from "svelte";
    import RecoveryCodes from "./RecoveryCodes.svelte";

    type Props = { replace?: boolean; done?: () => void };
    let { replace = false, done }: Props = $props();

    let setup = $state<TwoFactorSetup | null>(null);
    let loading = $state(true);
    let password = $state("");
    let code = $state("");
    let current = $state("");
    let currentRecovery = $state(false);
    let busy = $state(false);
    let failure = $state("");
    let codes = $state<string[]>([]);
    let showing = $state(false);
    let enrolled: PanelUser | null = null;

    const held = $derived.by((): SecondFactor | null => {
        if (!replace) return null;
        if (currentRecovery) {
            const read = readRecoveryCode(current);
            return read === null ? null : { recovery_code: read };
        }
        const read = readCode(current);
        return read === null ? null : { code: read };
    });

    const reason = $derived.by(() => {
        if (password === "") return "Enter your password.";
        if (replace && held === null)
            return currentRecovery ? "Enter one recovery code, ten characters." : "Enter the six digits the app you use now shows.";
        if (readCode(code) === null)
            return replace ? "Enter the six digits the new app shows." : "Enter the six digits your authenticator app shows.";
        return null;
    });

    async function load(fresh: boolean) {
        loading = true;
        failure = "";
        try {
            setup = await setUpTwoFactor(fresh);
        } catch (refusal) {
            failure = refusal instanceof ApiError ? refusal.message : "The panel could not set up two-factor sign-in.";
        } finally {
            loading = false;
        }
    }

    onMount(() => {
        void load(replace);
    });

    function describe(error: ApiError): string {
        if (error.code === "check_refused" && replace)
            return "That password and those codes do not confirm it is you. Check each code comes from the app it is asked for.";
        if (error.code === "check_refused")
            return "That password and code do not confirm it is you. Check the code is from the account you just added.";
        if (error.code === "not_set_up") return "The secret being set up has gone; a new one is shown below.";
        if (error.status === 429) return "Too many attempts. Wait a moment and try again.";
        if (error.status === 0) return "The panel could not reach its server. Check that it is still running.";
        return Object.values(error.fields)[0] ?? error.message;
    }

    async function enable(event: SubmitEvent) {
        event.preventDefault();
        if (reason !== null || busy) return;
        busy = true;
        failure = "";
        try {
            const answer = await turnOnTwoFactor(password, readCode(code) ?? code, held);
            password = "";
            code = "";
            current = "";
            enrolled = answer.user ?? null;
            codes = answer.recovery_codes;
            showing = true;
        } catch (refusal) {
            if (refusal instanceof ApiError) {
                failure = describe(refusal);
                if (refusal.code === "not_set_up") void load(true);
            } else {
                failure = "Two-factor sign-in could not be turned on.";
            }
            code = "";
            current = "";
        } finally {
            busy = false;
        }
    }

    function saved() {
        codes = [];
        setup = null;
        if (enrolled) adoptUser(enrolled);
        enrolled = null;
        done?.();
    }
</script>

<Card.Root class="shadow-xs">
    <Card.Header>
        <Card.Title class="flex items-center gap-2"><SmartphoneIcon class="size-4" />Add this panel to your authenticator app</Card.Title>
        <Card.Description>
            Any app that makes six-digit codes every thirty seconds works, such as the one your phone or password manager already has.
        </Card.Description>
    </Card.Header>
    <Card.Content class="space-y-4">
        {#if failure}
            <p class="rounded-md border border-destructive/30 bg-destructive/5 p-3 text-sm text-destructive" role="alert">{failure}</p>
        {/if}
        {#if loading}
            <p class="text-sm text-muted-foreground" aria-busy="true">Making a secret for you</p>
        {:else if setup}
            <div class="space-y-2">
                <p class="text-sm">In the app, add an account and type this key, time based:</p>
                <p class="rounded-lg border bg-muted/40 p-3 font-mono text-base tracking-wide select-all" aria-label="Setup key">
                    {groupSecret(setup.secret)}
                </p>
                <p class="text-xs text-muted-foreground">
                    Account <span class="font-mono">{setup.account}</span> on <span class="font-mono">{setup.issuer}</span>. On the device
                    your app runs on, <a class="text-primary underline-offset-4 hover:underline" href={setup.uri}>open this link</a> to add it
                    in one step.
                </p>
            </div>
            <form class="space-y-4" onsubmit={enable} novalidate>
                <div class="space-y-2">
                    <Label for="two-factor-password">Password</Label>
                    <Input id="two-factor-password" type="password" autocomplete="current-password" bind:value={password} />
                </div>
                {#if replace}
                    <div class="space-y-2">
                        <Label for="two-factor-current">{currentRecovery ? "Recovery code" : "Code from the app you use now"}</Label>
                        <Input
                            id="two-factor-current"
                            autocomplete="one-time-code"
                            inputmode={currentRecovery ? "text" : "numeric"}
                            spellcheck="false"
                            class="font-mono"
                            bind:value={current}
                        />
                        <Button
                            variant="link"
                            class="h-auto px-0"
                            onclick={() => {
                                currentRecovery = !currentRecovery;
                                current = "";
                            }}>{currentRecovery ? "Use a code from the app you use now instead" : "Use a recovery code instead"}</Button
                        >
                    </div>
                {/if}
                <div class="space-y-2">
                    <Label for="two-factor-code">{replace ? "Code from the new app" : "Code from the app"}</Label>
                    <Input
                        id="two-factor-code"
                        autocomplete="one-time-code"
                        inputmode="numeric"
                        spellcheck="false"
                        class="font-mono"
                        bind:value={code}
                    />
                </div>
                <div class="flex flex-wrap items-center gap-3">
                    <Button type="submit" disabled={busy || reason !== null}
                        ><ShieldCheckIcon />{busy ? "Turning it on" : "Turn on two-factor sign-in"}</Button
                    >
                    {#if reason}<p class="text-xs text-muted-foreground">{reason}</p>{/if}
                </div>
            </form>
        {:else}
            <Button variant="outline" onclick={() => void load(replace)}>Try again</Button>
        {/if}
    </Card.Content>
</Card.Root>

<RecoveryCodes bind:open={showing} {codes} account={setup?.account ?? ""} issuer={setup?.issuer ?? ""} {saved} />
