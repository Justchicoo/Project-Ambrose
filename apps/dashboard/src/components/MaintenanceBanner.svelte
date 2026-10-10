<!-- Project Ambrose by Imjustchico: The banner the panel shows while installation maintenance is active: the current window with a dialog to enter or leave maintenance. -->
<script lang="ts">
    import { Button } from "$lib/components/ui/button/index.js";
    import { session } from "$lib/api.svelte.js";
    import { may } from "$lib/permission.svelte.js";
    import { describeWindow, formatInstant, getMaintenance } from "$lib/maintenance.svelte.js";
    import type { MaintenanceState } from "$lib/schemas.js";
    import TriangleAlertIcon from "@lucide/svelte/icons/triangle-alert";
    import MaintenanceDialog from "./MaintenanceDialog.svelte";

    let maintenance = $state<MaintenanceState | null>(null);
    let dialogOpen = $state(false);

    const signedIn = $derived(session.state === "signed-in" && session.panel);
    const canControl = $derived(may("panel.maintenance"));

    async function refresh(signal?: AbortSignal) {
        if (!signedIn) return;
        try {
            maintenance = await getMaintenance(signal);
        } catch {
            maintenance = null;
        }
    }

    $effect(() => {
        if (!signedIn) return;
        const controller = new AbortController();
        void refresh(controller.signal);
        const timer = window.setInterval(() => void refresh(), 30000);
        return () => {
            controller.abort();
            window.clearInterval(timer);
        };
    });

    const windowText = $derived(maintenance ? describeWindow(maintenance) : null);

    function adopt(next: MaintenanceState) {
        maintenance = next;
    }

    function openDialog() {
        dialogOpen = true;
    }
</script>

{#if maintenance?.active}
    <div class="flex flex-wrap items-center gap-3 rounded-lg border border-amber-500/40 bg-amber-500/10 px-4 py-3 text-sm" role="alert">
        <TriangleAlertIcon class="size-4 shrink-0 text-amber-600 dark:text-amber-400" />
        <p class="min-w-0 flex-1">
            <strong class="font-semibold">Maintenance mode.</strong>
            {maintenance.reason}
            <span class="text-muted-foreground">
                Closed by {maintenance.started_by} at {formatInstant(maintenance.started_epoch_ms)}{#if windowText}, window {windowText}{/if}.
            </span>
        </p>
        {#if canControl}
            <Button size="sm" variant="outline" onclick={openDialog}>End maintenance</Button>
        {/if}
    </div>
    <MaintenanceDialog mode="exit" bind:open={dialogOpen} done={adopt} />
{/if}
