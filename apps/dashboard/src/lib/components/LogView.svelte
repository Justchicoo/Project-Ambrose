<!-- Project Ambrose by Imjustchico: Captured output drawn as the four columns doc/DESIGN.md settles, as one grid so every message begins at the same place and the eye reads down a column rather than hunting for the start of each line. The date is dropped because every line repeats it, the time brightens on the first line of a new second so a burst reads as a group, a category longer than its column is shortened in the middle rather than cut off, a message wraps under itself instead of under the time, and a line the supervisor wrote about an app is left whole in its own colour. -->
<script lang="ts">
    import { readLine, type LinePart } from "$lib/logline.js";

    type Line = { seq: number; stream: string; text: string };
    type Props = { lines: Line[]; label?: string };
    let { lines, label = "Captured output" }: Props = $props();

    const CategoryWidth = 20;

    const levelColour: Record<string, string> = {
        TRACE: "text-faint",
        DEBUG: "text-faint",
        INFO: "text-healthy",
        WARN: "text-waiting",
        ERROR: "text-destructive",
        FATAL: "text-destructive font-semibold",
    };

    const valueColour: Record<LinePart["kind"], string> = {
        number: "text-value-number",
        text: "text-value-text",
        name: "text-value-name",
        address: "text-value-address",
        setting: "text-value-setting",
        plain: "",
    };

    function shorten(category: string): string {
        if (category.length <= CategoryWidth) return category;
        const head = Math.ceil((CategoryWidth - 1) / 2);
        return `${category.slice(0, head)}…${category.slice(category.length - (CategoryWidth - 1 - head))}`;
    }

    const rows = $derived.by(() => {
        let second = "";
        return lines.map((line) => {
            const record = readLine(line.text);
            const at = record ? record.time.slice(0, 8) : "";
            const fresh = record !== null && at !== second;
            if (record) second = at;
            return { line, record, fresh, quiet: record !== null && (record.level === "DEBUG" || record.level === "TRACE") };
        });
    });
</script>

<div class="max-h-96 overflow-auto rounded-md border bg-muted/30 p-3 font-mono text-xs leading-relaxed" role="log" aria-label={label}>
    <div class="grid grid-cols-[auto_auto_auto_minmax(0,1fr)] gap-x-3 gap-y-0.5">
        {#each rows as row (row.line.seq)}
            {#if !row.record}
                <div
                    class="col-span-4 whitespace-pre-wrap {row.line.stream === 'stderr'
                        ? 'text-destructive'
                        : row.line.stream === 'supervisor'
                          ? 'text-mine'
                          : 'text-foreground'}"
                >
                    {row.line.text}
                </div>
            {:else}
                <span class="tabular-nums {row.fresh && !row.quiet ? 'text-muted-foreground' : 'text-faint'}">{row.record.time}</span>
                <span class="w-10 {row.quiet ? 'text-faint' : (levelColour[row.record.level] ?? 'text-foreground')}"
                    >{row.record.level}</span
                >
                <span class="text-muted-foreground" title={row.record.category}>[{shorten(row.record.category)}]</span>
                <span class="min-w-0 break-words {row.quiet ? 'text-faint' : 'text-foreground'}"
                    >{#each row.record.parts as part, index (index)}<span class={row.quiet ? "" : valueColour[part.kind]}>{part.text}</span
                        >{/each}</span
                >
            {/if}
        {/each}
    </div>
</div>
