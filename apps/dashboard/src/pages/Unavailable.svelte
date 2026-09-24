<!-- Project Ambrose by Imjustchico: What a page shows before the milestone that builds it has landed: its name, the milestone that brings it, and on request a design preview drawn from made-up figures under a banner that says so, so nothing on it passes for this server's own data. -->
<script lang="ts">
    import { Button } from "$lib/components/ui/button/index.js";
    import ClockIcon from "@lucide/svelte/icons/clock";
    import EyeIcon from "@lucide/svelte/icons/eye";
    import EyeOffIcon from "@lucide/svelte/icons/eye-off";
    import type { Preview } from "../routes";

    type Props = { path: string; title: string; milestone: string; preview: Preview };
    let { path, title, milestone, preview }: Props = $props();

    let previewing = $state(false);
</script>

{#if previewing}
    <div class="flex flex-wrap items-center gap-3 rounded-lg border border-waiting/40 bg-waiting/10 px-4 py-3 text-sm" role="status">
        <EyeIcon class="size-4 shrink-0 text-waiting" />
        <p class="flex-1">Design preview with made-up figures. The real {title} page arrives with milestone {milestone}.</p>
        <Button size="sm" variant="outline" onclick={() => (previewing = false)}><EyeOffIcon />Close the preview</Button>
    </div>
    {#if preview.kind === "list"}
        {#await preview.load() then loaded}
            <loaded.default which={path} {title} />
        {:catch}
            <p class="text-sm text-muted-foreground">The preview could not be loaded. Reload the panel to fetch it again.</p>
        {/await}
    {:else}
        {#await preview.load() then loaded}
            <loaded.default />
        {:catch}
            <p class="text-sm text-muted-foreground">The preview could not be loaded. Reload the panel to fetch it again.</p>
        {/await}
    {/if}
{:else}
    <div class="flex flex-1 flex-col items-center justify-center gap-4 rounded-xl border border-dashed p-12 text-center">
        <div class="flex size-12 items-center justify-center rounded-full bg-muted"><ClockIcon class="size-5 text-muted-foreground" /></div>
        <div class="space-y-1">
            <h1 class="font-serif text-2xl font-semibold">{title}</h1>
            <p class="max-w-sm text-sm text-muted-foreground">
                This page arrives with milestone {milestone}. Until then nothing here reads this server.
            </p>
        </div>
        <Button variant="outline" onclick={() => (previewing = true)}><EyeIcon />Show the design preview</Button>
    </div>
{/if}
