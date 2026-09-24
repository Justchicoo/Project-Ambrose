<!-- Project Ambrose by Imjustchico: The page a link lands on when the viewer may not use its route or no route matches, with the way back to the overview. -->
<script lang="ts">
    import { Button } from "$lib/components/ui/button/index.js";
    import ArrowLeftIcon from "@lucide/svelte/icons/arrow-left";
    import LockIcon from "@lucide/svelte/icons/lock";

    type Props = { title: string; detail: string };
    let { title, detail }: Props = $props();
</script>

<div class="flex flex-1 flex-col items-center justify-center gap-4 rounded-xl border border-dashed p-12 text-center">
    <div class="flex size-12 items-center justify-center rounded-full bg-muted"><LockIcon class="size-5 text-muted-foreground" /></div>
    <div class="space-y-1">
        <h1 class="font-serif text-2xl font-semibold">{title}</h1>
        <p class="max-w-sm text-sm text-muted-foreground">{detail}</p>
    </div>
    <Button variant="outline" href="#overview"><ArrowLeftIcon />Back to the overview</Button>
</div>
