<!-- Project Ambrose by Imjustchico: The one dialog every installation maintenance change goes through: entering takes the reason players are refused with and an optional window the public status page will publish, leaving is confirmed in one step, a refusal from the server is shown with its own message, and the step-up check the danger permission asks for is retried once through the shared prompt. -->
<script lang="ts">
    import * as Dialog from "$lib/components/ui/dialog/index.js";
    import { Button } from "$lib/components/ui/button/index.js";
    import { Input } from "$lib/components/ui/input/index.js";
    import { Label } from "$lib/components/ui/label/index.js";
    import { Textarea } from "$lib/components/ui/textarea/index.js";
    import { ApiError } from "$lib/api.svelte.js";
    import { enterMaintenance, exitMaintenance } from "$lib/maintenance.svelte.js";
    import type { MaintenanceState } from "$lib/schemas.js";
    import { textBytes } from "$lib/settings.js";

    type Props = {
        open: boolean;
        mode: "enter" | "exit";
        done: (next: MaintenanceState) => void;
    };

    let { open = $bindable(), mode, done }: Props = $props();

    const MaxReasonBytes = 255;
    let reason = $state("");
    let windowStart = $state("");
    let windowEnd = $state("");
    let busy = $state(false);
    let failure = $state("");

    $effect(() => {
        if (open) {
            reason = "";
            windowStart = "";
            windowEnd = "";
            busy = false;
            failure = "";
        }
    });

    const reasonProblem = $derived(
        mode === "enter" && reason.trim() === ""
            ? "Say why the installation closes, so players and the audit row can tell"
            : mode === "enter" && textBytes(reason.trim()) > MaxReasonBytes
              ? `A reason is at most ${MaxReasonBytes} bytes`
              : null,
    );

    const windowProblem = $derived.by(() => {
        if (mode !== "enter") return null;
        if (windowStart === "" && windowEnd === "") return null;
        if (windowStart === "" || windowEnd === "") return "A window takes both its start and its end";
        const start = new Date(windowStart).getTime();
        const end = new Date(windowEnd).getTime();
        if (Number.isNaN(start) || Number.isNaN(end)) return "The window is not two dates";
        if (start >= end) return "The window starts after it ends";
        return null;
    });

    const ready = $derived(mode === "exit" || (reasonProblem === null && windowProblem === null));

    async function apply() {
        if (!ready || busy) return;
        busy = true;
        failure = "";
        try {
            const start = windowStart === "" ? null : new Date(windowStart).getTime();
            const end = windowEnd === "" ? null : new Date(windowEnd).getTime();
            const next = mode === "enter" ? await enterMaintenance(reason.trim(), start, end) : await exitMaintenance();
            open = false;
            done(next);
        } catch (refusal) {
            if (refusal instanceof ApiError) failure = refusal.message;
            else failure = mode === "enter" ? "Maintenance could not be entered" : "Maintenance could not be left";
        } finally {
            busy = false;
        }
    }
</script>

<Dialog.Root bind:open>
    <Dialog.Content class="sm:max-w-lg">
        <Dialog.Header>
            <Dialog.Title>
                {#if mode === "enter"}Close the installation for maintenance?{:else}Open the installation again?{/if}
            </Dialog.Title>
            <Dialog.Description>
                {#if mode === "enter"}
                    Players below the bypass level are refused at sign-in with the reason, while game masters sign in normally and players
                    already in the world stay connected.
                {:else}
                    Players sign in normally again and the maintenance record is cleared.
                {/if}
            </Dialog.Description>
        </Dialog.Header>

        {#if failure}
            <p class="rounded-md border border-destructive/30 bg-destructive/5 p-3 text-sm text-destructive" role="alert">{failure}</p>
        {/if}

        {#if mode === "enter"}
            <div class="space-y-4">
                <div class="space-y-2">
                    <Label for="maintenance-reason">Why</Label>
                    <Textarea
                        id="maintenance-reason"
                        bind:value={reason}
                        rows={2}
                        placeholder="Database upgrade"
                        aria-invalid={reasonProblem !== null && reason !== ""}
                    />
                    {#if reasonProblem && reason !== ""}<p class="text-xs text-destructive">{reasonProblem}</p>{/if}
                </div>
                <div class="grid grid-cols-2 gap-4">
                    <div class="space-y-2">
                        <Label for="maintenance-window-start">Window starts (optional)</Label>
                        <Input id="maintenance-window-start" type="datetime-local" bind:value={windowStart} />
                    </div>
                    <div class="space-y-2">
                        <Label for="maintenance-window-end">Window ends (optional)</Label>
                        <Input id="maintenance-window-end" type="datetime-local" bind:value={windowEnd} />
                    </div>
                </div>
                {#if windowProblem}<p class="text-xs text-destructive" role="alert">{windowProblem}</p>{/if}
                <p class="text-xs text-muted-foreground">
                    The window is published to the public status page. The change is audited with who, why and the window.
                </p>
            </div>
        {:else}
            <p class="text-sm text-muted-foreground">Leaving maintenance is audited with who ended it.</p>
        {/if}

        <Dialog.Footer>
            <Button variant="outline" onclick={() => (open = false)}>Leave it</Button>
            <Button disabled={!ready || busy} onclick={() => void apply()}>
                {#if busy}Working…{:else if mode === "enter"}Close it{:else}Open it{/if}
            </Button>
        </Dialog.Footer>
    </Dialog.Content>
</Dialog.Root>
