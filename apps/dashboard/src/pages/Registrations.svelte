<!-- Project Ambrose by Imjustchico: The operator's audited view of recent player registrations, with verification status, resend for pending email verification and account blocking. -->
<script lang="ts">
    import * as Card from "$lib/components/ui/card/index.js";
    import { Button } from "$lib/components/ui/button/index.js";
    import { ApiError, blockPlayerRegistration, playerRegistrations, resendPlayerVerification } from "$lib/api.svelte.js";
    import type { InferOutput } from "valibot";
    import { PlayerRegistrationsAnswer } from "$lib/schemas.js";
    import PageHeader from "../components/PageHeader.svelte";
    import StatusBadge from "../components/StatusBadge.svelte";

    type Registration = InferOutput<typeof PlayerRegistrationsAnswer>["registrations"][number];
    let registrations = $state<Registration[]>([]);
    let failure = $state("");
    let notice = $state("");
    let busyId = $state<number | null>(null);

    async function refresh() {
        failure = "";
        try {
            registrations = (await playerRegistrations()).registrations;
        } catch (problem) {
            failure = problem instanceof ApiError ? problem.message : "Player registrations could not be read";
        }
    }

    async function resend(registration: Registration) {
        busyId = registration.account_id;
        failure = "";
        notice = "";
        try {
            await resendPlayerVerification(registration.account_id);
            notice = `A verification link was sent for ${registration.username}.`;
            await refresh();
        } catch (problem) {
            failure = problem instanceof ApiError ? problem.message : "The verification link could not be sent";
        } finally {
            busyId = null;
        }
    }

    async function block(registration: Registration) {
        busyId = registration.account_id;
        failure = "";
        notice = "";
        try {
            await blockPlayerRegistration(registration.account_id);
            notice = `${registration.username} was blocked.`;
            await refresh();
        } catch (problem) {
            failure = problem instanceof ApiError ? problem.message : "The registration could not be blocked";
        } finally {
            busyId = null;
        }
    }

    $effect(() => {
        void refresh();
    });
</script>

<PageHeader title="Player registrations" description="Review recent sign-ups and manage email verification or account access.">
    {#snippet actions()}
        <Button variant="outline" onclick={() => void refresh()}>Refresh</Button>
    {/snippet}
</PageHeader>

{#if failure}
    <p class="mb-4 rounded-md border border-destructive/30 bg-destructive/5 p-3 text-sm text-destructive" role="alert">{failure}</p>
{/if}
{#if notice}
    <p class="mb-4 rounded-md border border-healthy/30 bg-healthy/5 p-3 text-sm text-healthy" role="status">{notice}</p>
{/if}

<Card.Root class="shadow-xs">
    <Card.Header>
        <Card.Title>Recent sign-ups</Card.Title>
        <Card.Description>At most 200 registrations are shown. Email addresses are not displayed here.</Card.Description>
    </Card.Header>
    <Card.Content>
        {#if registrations.length === 0}
            <p class="text-sm text-muted-foreground">No player registrations are recorded.</p>
        {:else}
            <div class="overflow-x-auto">
                <table class="w-full text-left text-sm">
                    <thead>
                        <tr class="border-b text-muted-foreground">
                            <th class="p-2">Account</th>
                            <th class="p-2">State</th>
                            <th class="p-2">Created</th>
                            <th class="p-2">Actions</th>
                        </tr>
                    </thead>
                    <tbody>
                        {#each registrations as registration (registration.account_id)}
                            <tr class="border-b last:border-0">
                                <td class="p-2 font-medium">{registration.username}</td>
                                <td class="p-2">
                                    <StatusBadge
                                        tone={
                                            registration.state === "verified"
                                                ? "healthy"
                                                : registration.state === "pending"
                                                  ? "waiting"
                                                  : "wrong"
                                        }
                                    >
                                        {registration.state}
                                    </StatusBadge>
                                </td>
                                <td class="p-2">{new Date(registration.created_epoch_ms).toLocaleString()}</td>
                                <td class="p-2">
                                    <div class="flex flex-wrap gap-2">
                                        {#if registration.state === "pending"}
                                            <Button
                                                size="sm"
                                                variant="outline"
                                                disabled={busyId !== null}
                                                onclick={() => void resend(registration)}
                                            >
                                                Resend
                                            </Button>
                                        {/if}
                                        {#if registration.state !== "blocked"}
                                            <Button
                                                size="sm"
                                                variant="destructive"
                                                disabled={busyId !== null}
                                                onclick={() => void block(registration)}
                                            >
                                                Block
                                            </Button>
                                        {/if}
                                    </div>
                                </td>
                            </tr>
                        {/each}
                    </tbody>
                </table>
            </div>
        {/if}
    </Card.Content>
</Card.Root>
