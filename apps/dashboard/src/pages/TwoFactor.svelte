<!-- Project Ambrose by Imjustchico: The operator's own two-factor sign-in, reached from the user menu: whether it is on and since when, and whether the panel requires it of them; turning it on through the setup that shows the recovery codes once; and, while it is on, how many recovery codes are left, ten new ones for the password and a current code, shown once, moving to another authenticator app, which takes a current code or recovery code from the app in use as well as one from the new app, and turning it off for the password and a current code, which ends the operator's other sessions and is refused while the panel requires it. -->
<script lang="ts">
    import * as Card from "$lib/components/ui/card/index.js";
    import { Button } from "$lib/components/ui/button/index.js";
    import { Input } from "$lib/components/ui/input/index.js";
    import { Label } from "$lib/components/ui/label/index.js";
    import { Skeleton } from "$lib/components/ui/skeleton/index.js";
    import { ApiError, newRecoveryCodes, session, turnOffTwoFactor, twoFactorState, type SecondFactor } from "$lib/api.svelte.js";
    import { readCode, readRecoveryCode } from "$lib/twofactor.js";
    import type { TwoFactorState } from "$lib/schemas.js";
    import KeyRoundIcon from "@lucide/svelte/icons/key-round";
    import ShieldOffIcon from "@lucide/svelte/icons/shield-off";
    import SmartphoneIcon from "@lucide/svelte/icons/smartphone";
    import { onMount } from "svelte";
    import PageHeader from "../components/PageHeader.svelte";
    import RecoveryCodes from "../components/RecoveryCodes.svelte";
    import StatusBadge from "../components/StatusBadge.svelte";
    import TwoFactorEnroll from "../components/TwoFactorEnroll.svelte";

    let standing = $state<TwoFactorState | null>(null);
    let failure = $state("");
    let moving = $state(false);

    let freshPassword = $state("");
    let freshValue = $state("");
    let freshRecovery = $state(false);
    let freshBusy = $state(false);
    let freshFailure = $state("");
    let codes = $state<string[]>([]);
    let showing = $state(false);

    let offPassword = $state("");
    let offValue = $state("");
    let offRecovery = $state(false);
    let offBusy = $state(false);
    let offFailure = $state("");
    let offNotice = $state("");

    const account = $derived(session.user?.username ?? "");

    function factorOf(value: string, recovery: boolean): SecondFactor | null {
        if (recovery) {
            const read = readRecoveryCode(value);
            return read === null ? null : { recovery_code: read };
        }
        const read = readCode(value);
        return read === null ? null : { code: read };
    }

    function reasonOf(password: string, value: string, recovery: boolean): string | null {
        if (password === "") return "Enter your password.";
        if (factorOf(value, recovery) === null)
            return recovery ? "Enter one recovery code, ten characters." : "Enter the six digits your authenticator app shows.";
        return null;
    }

    const freshReason = $derived(reasonOf(freshPassword, freshValue, freshRecovery));
    const offReason = $derived(reasonOf(offPassword, offValue, offRecovery));

    function describe(error: ApiError): string {
        if (error.code === "check_refused") return "That password and code do not confirm it is you.";
        if (error.code === "two_factor_required") return "This panel requires two-factor sign-in for your account, so it stays on.";
        if (error.status === 429) return "Too many attempts. Wait a moment and try again.";
        if (error.status === 0) return "The panel could not reach its server. Check that it is still running.";
        return Object.values(error.fields)[0] ?? error.message;
    }

    async function load() {
        failure = "";
        try {
            standing = await twoFactorState();
        } catch (refusal) {
            failure = refusal instanceof ApiError ? refusal.message : "Your two-factor sign-in could not be read.";
        }
    }

    onMount(() => {
        void load();
    });

    async function regenerate(event: SubmitEvent) {
        event.preventDefault();
        const factor = factorOf(freshValue, freshRecovery);
        if (freshReason !== null || factor === null || freshBusy) return;
        freshBusy = true;
        freshFailure = "";
        try {
            const answer = await newRecoveryCodes(freshPassword, factor);
            freshPassword = "";
            freshValue = "";
            codes = answer.recovery_codes;
            showing = true;
        } catch (refusal) {
            freshFailure = refusal instanceof ApiError ? describe(refusal) : "New recovery codes could not be made.";
            freshValue = "";
        } finally {
            freshBusy = false;
        }
    }

    async function turnOff(event: SubmitEvent) {
        event.preventDefault();
        const factor = factorOf(offValue, offRecovery);
        if (offReason !== null || factor === null || offBusy) return;
        offBusy = true;
        offFailure = "";
        offNotice = "";
        try {
            await turnOffTwoFactor(offPassword, factor);
            offPassword = "";
            offValue = "";
            offNotice = "Two-factor sign-in is off, and every other session you had has ended.";
            await load();
        } catch (refusal) {
            offFailure = refusal instanceof ApiError ? describe(refusal) : "Two-factor sign-in could not be turned off.";
            offValue = "";
        } finally {
            offBusy = false;
        }
    }

    function codesSaved() {
        codes = [];
        void load();
    }

    function enrolled() {
        moving = false;
        offNotice = "";
        void load();
    }
</script>

<PageHeader
    title="Two-factor sign-in"
    description="A code from an authenticator app as well as your password, so a password alone never opens your account."
/>

{#if failure}
    <p class="rounded-md border border-destructive/30 bg-destructive/5 p-3 text-sm text-destructive" role="alert">{failure}</p>
{/if}
{#if offNotice}
    <p class="rounded-md border border-healthy/30 bg-healthy/5 p-3 text-sm text-healthy" role="status">{offNotice}</p>
{/if}

{#if standing === null}
    {#if !failure}
        <div class="space-y-3" aria-busy="true" aria-label="Loading your two-factor sign-in">
            <Skeleton class="h-24 w-full" />
            <Skeleton class="h-48 w-full" />
        </div>
    {/if}
{:else}
    <Card.Root class="shadow-xs">
        <Card.Header>
            <Card.Title class="flex flex-wrap items-center gap-2">
                Your account
                {#if standing.enabled}
                    <StatusBadge tone="healthy">On</StatusBadge>
                {:else}
                    <StatusBadge tone={standing.required ? "wrong" : "unknown"}>Off</StatusBadge>
                {/if}
                {#if standing.required}<StatusBadge tone="waiting">Required by this panel</StatusBadge>{/if}
            </Card.Title>
            <Card.Description>
                {#if standing.enabled}
                    On since {standing.enabled_epoch_ms ? new Date(standing.enabled_epoch_ms).toLocaleString() : "it was turned on"}, with {standing.recovery_codes_left}
                    of 10 recovery codes left.
                {:else if standing.required}
                    This panel requires it for your account. Turn it on below.
                {:else}
                    Signing in takes only your password. Turning this on adds a code from an app on your phone or computer.
                {/if}
            </Card.Description>
        </Card.Header>
    </Card.Root>

    {#if !standing.enabled || moving}
        <TwoFactorEnroll replace={moving} done={enrolled} />
        {#if moving}
            <div class="flex justify-end">
                <Button variant="ghost" onclick={() => (moving = false)}>Keep the app I have</Button>
            </div>
        {/if}
    {:else}
        <Card.Root class="shadow-xs">
            <Card.Header>
                <Card.Title class="flex items-center gap-2"><KeyRoundIcon class="size-4" />Recovery codes</Card.Title>
                <Card.Description>
                    {standing.recovery_codes_left} of 10 left. New codes replace every earlier one, and are shown once.
                </Card.Description>
            </Card.Header>
            <Card.Content>
                <form class="space-y-4" onsubmit={regenerate} novalidate>
                    {#if freshFailure}
                        <p class="rounded-md border border-destructive/30 bg-destructive/5 p-3 text-sm text-destructive" role="alert">
                            {freshFailure}
                        </p>
                    {/if}
                    <div class="grid gap-4 sm:grid-cols-2">
                        <div class="space-y-2">
                            <Label for="fresh-codes-password">Password</Label>
                            <Input id="fresh-codes-password" type="password" autocomplete="current-password" bind:value={freshPassword} />
                        </div>
                        <div class="space-y-2">
                            <Label for="fresh-codes-code">{freshRecovery ? "Recovery code" : "Code from your authenticator app"}</Label>
                            <Input
                                id="fresh-codes-code"
                                autocomplete="one-time-code"
                                inputmode={freshRecovery ? "text" : "numeric"}
                                spellcheck="false"
                                class="font-mono"
                                bind:value={freshValue}
                            />
                        </div>
                    </div>
                    <div class="flex flex-wrap items-center gap-3">
                        <Button type="submit" disabled={freshBusy || freshReason !== null}
                            >{freshBusy ? "Making codes" : "Make new recovery codes"}</Button
                        >
                        <Button
                            variant="link"
                            class="h-auto px-0"
                            onclick={() => {
                                freshRecovery = !freshRecovery;
                                freshValue = "";
                            }}>{freshRecovery ? "Use a code from the app instead" : "Use a recovery code instead"}</Button
                        >
                        {#if freshReason}<p class="text-xs text-muted-foreground">{freshReason}</p>{/if}
                    </div>
                </form>
            </Card.Content>
        </Card.Root>

        <Card.Root class="shadow-xs">
            <Card.Header>
                <Card.Title class="flex items-center gap-2"><SmartphoneIcon class="size-4" />Another authenticator app</Card.Title>
                <Card.Description>
                    Moving to a new phone or app sets up a new secret beside the one you use now; the old app keeps working until a code
                    from it and a code from the new one turn the new one on, which also makes new recovery codes and ends your other
                    sessions.
                </Card.Description>
            </Card.Header>
            <Card.Content>
                <Button variant="outline" onclick={() => (moving = true)}>Move to another app</Button>
            </Card.Content>
        </Card.Root>

        <Card.Root class="shadow-xs">
            <Card.Header>
                <Card.Title class="flex items-center gap-2"><ShieldOffIcon class="size-4" />Turn it off</Card.Title>
                <Card.Description>
                    Turning it off deletes your secret and every recovery code, and ends every other session you have. It takes your
                    password and a current code.
                </Card.Description>
            </Card.Header>
            <Card.Content>
                {#if standing.required}
                    <p class="text-sm text-muted-foreground">This panel requires two-factor sign-in for your account, so it stays on.</p>
                {:else}
                    <form class="space-y-4" onsubmit={turnOff} novalidate>
                        {#if offFailure}
                            <p class="rounded-md border border-destructive/30 bg-destructive/5 p-3 text-sm text-destructive" role="alert">
                                {offFailure}
                            </p>
                        {/if}
                        <div class="grid gap-4 sm:grid-cols-2">
                            <div class="space-y-2">
                                <Label for="turn-off-password">Password</Label>
                                <Input id="turn-off-password" type="password" autocomplete="current-password" bind:value={offPassword} />
                            </div>
                            <div class="space-y-2">
                                <Label for="turn-off-code">{offRecovery ? "Recovery code" : "Code from your authenticator app"}</Label>
                                <Input
                                    id="turn-off-code"
                                    autocomplete="one-time-code"
                                    inputmode={offRecovery ? "text" : "numeric"}
                                    spellcheck="false"
                                    class="font-mono"
                                    bind:value={offValue}
                                />
                            </div>
                        </div>
                        <div class="flex flex-wrap items-center gap-3">
                            <Button type="submit" variant="destructive" disabled={offBusy || offReason !== null}
                                >{offBusy ? "Turning it off" : "Turn off two-factor sign-in"}</Button
                            >
                            <Button
                                variant="link"
                                class="h-auto px-0"
                                onclick={() => {
                                    offRecovery = !offRecovery;
                                    offValue = "";
                                }}>{offRecovery ? "Use a code from the app instead" : "Use a recovery code instead"}</Button
                            >
                            {#if offReason}<p class="text-xs text-muted-foreground">{offReason}</p>{/if}
                        </div>
                    </form>
                {/if}
            </Card.Content>
        </Card.Root>
    {/if}
{/if}

<RecoveryCodes bind:open={showing} {codes} {account} issuer={standing?.issuer ?? ""} saved={codesSaved} />
