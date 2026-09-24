<!-- Project Ambrose by Imjustchico: A shadcn badge that names a state in words with a matching dot and a tinted wash, so healthy, waiting, wrong and unknown read the same on every page. -->
<script lang="ts">
    import { Badge } from "$lib/components/ui/badge/index.js";
    import { cn } from "$lib/utils.js";
    import type { Snippet } from "svelte";

    type Tone = "healthy" | "waiting" | "wrong" | "unknown" | "mine";
    type Props = { tone: Tone; pulse?: boolean; class?: string; children: Snippet };
    let { tone, pulse = false, class: extra, children }: Props = $props();

    const looks: Record<Tone, { badge: string; dot: string }> = {
        healthy: { badge: "border-healthy/30 bg-healthy/10 text-healthy", dot: "bg-healthy" },
        waiting: { badge: "border-waiting/30 bg-waiting/10 text-waiting", dot: "bg-waiting" },
        wrong: { badge: "border-destructive/30 bg-destructive/10 text-destructive", dot: "bg-destructive" },
        unknown: { badge: "border-border bg-muted text-muted-foreground", dot: "bg-unknown" },
        mine: { badge: "border-mine/30 bg-mine/10 text-mine", dot: "bg-mine" },
    };
</script>

<Badge variant="outline" class={cn("gap-1.5 font-medium", looks[tone].badge, extra)}>
    <span class="relative flex size-1.5">
        {#if pulse}<span class={cn("absolute inline-flex size-full animate-ping rounded-full opacity-60", looks[tone].dot)}></span>{/if}
        <span class={cn("relative inline-flex size-1.5 rounded-full", looks[tone].dot)}></span>
    </span>
    {@render children()}
</Badge>
