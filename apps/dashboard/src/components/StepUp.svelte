<!-- Project Ambrose by Imjustchico: The question the panel asks before a danger action when the operator's last check of who they are is older than the panel allows: a code from their authenticator app, or one recovery code instead, or their password when they have no second factor, sent as a fresh check that the action which asked is then sent again behind, while cancelling or closing it sends nothing more and leaves the action undone; every request that asks while it is open waits on the same answer. -->
<script lang="ts">
    import * as Dialog from "$lib/components/ui/dialog/index.js";
    import { Button } from "$lib/components/ui/button/index.js";
    import { Input } from "$lib/components/ui/input/index.js";
    import { Label } from "$lib/components/ui/label/index.js";
    import { ApiError, onStepUp, stepUp, type Confirmation } from "$lib/api.svelte.js";
    import { readCode, readRecoveryCode } from "$lib/twofactor.js";
    import type { StepUpAsked } from "$lib/schemas.js";
    import ShieldCheckIcon from "@lucide/svelte/icons/shield-check";
    import { onMount } from "svelte";

    let open = $state(false);
    let asked = $state<StepUpAsked | null>(null);
    let recovery = $state(false);
    let value = $state("");
    let busy = $state(false);
    let failure = $state("");
    let waiting: ((confirmed: boolean) => void)[] = [];

    const byPassword = $derived(asked !== null && !asked.methods.includes("totp"));
    const minutes = $derived(asked ? Math.max(1, Math.round(asked.window_seconds / 60)) : 0);
    const problem = $derived.by(() => {
        if (byPassword) return value === "" ? "Enter your password." : null;
        if (recovery) return readRecoveryCode(value) === null ? "Enter one recovery code, ten characters." : null;
        return readCode(value) === null ? "Enter the six digits your authenticator app shows." : null;
    });

    function finish(confirmed: boolean) {
        const answered = waiting;
        waiting = [];
        value = "";
        failure = "";
        recovery = false;
        open = false;
        for (const answer of answered) answer(confirmed);
    }

    onMount(() =>
        onStepUp((question) => {
            asked = question;
            open = true;
            return new Promise<boolean>((answer) => waiting.push(answer));
        }),
    );

    function describe(error: ApiError): string {
        if (error.code === "step_up_refused") return "That does not confirm it is you.";
        if (error.status === 429) return "Too many attempts. Wait a moment and try again.";
        if (error.status === 0) return "The panel could not reach its server. Check that it is still running.";
        return Object.values(error.fields)[0] ?? error.message;
    }

    async function confirm(event: SubmitEvent) {
        event.preventDefault();
        if (problem !== null || busy || asked === null) return;
        busy = true;
        failure = "";
        const confirmation: Confirmation = byPassword
            ? { password: value }
            : recovery
              ? { recovery_code: readRecoveryCode(value) ?? value }
              : { code: readCode(value) ?? value };
        try {
            await stepUp(confirmation, `use ${asked.permission}`);
            finish(true);
        } catch (refusal) {
            failure = refusal instanceof ApiError ? describe(refusal) : "The check could not be made.";
            value = "";
        } finally {
            busy = false;
        }
    }
</script>

<Dialog.Root
    bind:open
    onOpenChange={(next) => {
        if (!next) finish(false);
    }}
>
    <Dialog.Content class="sm:max-w-sm">
        <Dialog.Header>
            <Dialog.Title>Confirm it is you</Dialog.Title>
            <Dialog.Description>
                {#if asked}
                    This needs <span class="font-mono">{asked.permission}</span>, which the panel lets you use for {minutes} minute{minutes ===
                    1
                        ? ""
                        : "s"} after you confirm who you are.
                {/if}
            </Dialog.Description>
        </Dialog.Header>
        <form class="space-y-4" onsubmit={confirm} novalidate>
            {#if failure}
                <p class="rounded-md border border-destructive/30 bg-destructive/5 p-3 text-sm text-destructive" role="alert">{failure}</p>
            {/if}
            <div class="space-y-2">
                {#if byPassword}
                    <Label for="step-up-password">Password</Label>
                    <Input id="step-up-password" type="password" autocomplete="current-password" bind:value />
                {:else if recovery}
                    <Label for="step-up-recovery-code">Recovery code</Label>
                    <Input id="step-up-recovery-code" autocomplete="one-time-code" spellcheck="false" class="font-mono" bind:value />
                {:else}
                    <Label for="step-up-code">Code from your authenticator app</Label>
                    <Input
                        id="step-up-code"
                        autocomplete="one-time-code"
                        inputmode="numeric"
                        spellcheck="false"
                        class="font-mono"
                        bind:value
                    />
                {/if}
                {#if problem && value !== ""}<p class="text-xs text-muted-foreground">{problem}</p>{/if}
            </div>
            {#if !byPassword}
                <Button
                    variant="link"
                    class="h-auto px-0"
                    onclick={() => {
                        recovery = !recovery;
                        value = "";
                    }}>{recovery ? "Use a code from the app instead" : "Use a recovery code instead"}</Button
                >
            {/if}
            <Dialog.Footer>
                <Button variant="outline" onclick={() => finish(false)}>Cancel</Button>
                <Button type="submit" disabled={busy || problem !== null}><ShieldCheckIcon />{busy ? "Checking" : "Confirm"}</Button>
            </Dialog.Footer>
        </form>
    </Dialog.Content>
</Dialog.Root>
