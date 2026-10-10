<!-- Project Ambrose by Imjustchico: The installation maintenance card on the overview page: whether the installation is closed to players, who closed it, why, when it started and the optional window the public status page publishes, with the audited enter and leave controls for operators holding panel.maintenance. -->
<script lang="ts">
    import * as Card from "$lib/components/ui/card/index.js";
    import { Button } from "$lib/components/ui/button/index.js";
    import { Skeleton } from "$lib/components/ui/skeleton/index.js";
    import { session } from "$lib/api.svelte.js";
    import { may } from "$lib/permission.svelte.js";
    import { describeWindow, formatInstant, getMaintenance } from "$lib/maintenance.svelte.js";
    import type { MaintenanceState } from "$lib/schemas.js";
    import WrenchIcon from "@lucide/svelte/icons/wrench";
    import MaintenanceDialog from "./MaintenanceDialog.svelte";

    let maintenance = $state<MaintenanceState | null>(null);
    let failed = $state(false);
    let dialogMode = $state<"enter" | "exit">("enter");
    let dialogOpen = $state(false);

    const signedIn = $derived(session.state === "signed-in" && session.panel);
    const canControl = $derived(may("panel.maintenance"));
    const windowText = $derived(maintenance ? describeWindow(maintenance) : null);

    function adopt(next: MaintenanceState) {
        maintenance = next;
    }

    async function refresh(signal?: AbortSignal) {
        if (!signedIn) return;
        try {
            maintenance = await getMaintenance(signal);
            failed = false;
        } catch {
            failed = true;
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

    function ask(mode: "enter" | "exit") {
        dialogMode = mode;
        dialogOpen = true;
    }
</script>

<Card.Root class="gap-3 shadow-xs">
    <Card.Header>
        <Card.Title class="flex items-center gap-2 font-serif text-lg"><WrenchIcon class="size-4" />Installation maintenance</Card.Title>
        <Card.Description class="text-xs">Close the whole installation to players for database work.</Card.Description>
    </Card.Header>
    <Card.Content class="text-sm">
        {#if maintenance === null}
            {#if failed}
                <p class="text-muted-foreground">The maintenance state could not be read.</p>
            {:else}
                <Skeleton class="h-5 w-2/3" />
            {/if}
        {:else if maintenance.active}
            <p><strong class="font-semibold">Closed.</strong> {maintenance.reason}</p>
            <p class="mt-1 text-muted-foreground">
                By {maintenance.started_by} at {formatInstant(maintenance.started_epoch_ms)}{#if windowText}<br />Window {windowText}{/if}
            </p>
        {:else}
            <p class="text-muted-foreground">Open. Players sign in normally.</p>
        {/if}
    </Card.Content>
    {#if canControl && maintenance !== null}
        <Card.Footer>
            {#if maintenance.active}
                <Button size="sm" variant="outline" onclick={() => ask("exit")}>End maintenance</Button>
            {:else}
                <Button size="sm" variant="outline" onclick={() => ask("enter")}>Enter maintenance</Button>
            {/if}
        </Card.Footer>
    {/if}
</Card.Root>

<MaintenanceDialog mode={dialogMode} bind:open={dialogOpen} done={adopt} />
