<!-- Project Ambrose by Imjustchico: A live connection as a filled dot, the word Live and the age of the last sample, so it still says it is live when the dot cannot pulse. -->
<script lang="ts">
    import { classes } from "../internal/classes";

    type Props = {
        age: string;
        stale?: boolean;
        size?: "11" | "12" | "13" | "15";
        class?: string;
    };

    let { age, stale = false, size = "13", class: extra }: Props = $props();

    const sizes: Record<string, string> = {
        "11": "text-11",
        "12": "text-12",
        "13": "text-13",
        "15": "text-15",
    };
</script>

<span
    class={classes("inline-flex items-center gap-8", sizes[size], stale ? "text-state-unknown" : "text-state-healthy", extra)}
    data-live={stale ? "stale" : "live"}
>
    <span
        data-testid="live-dot"
        class={classes("h-8 w-8 shrink-0 rounded-pill", stale ? "bg-state-unknown" : "bg-state-healthy", stale ? null : "ambrose-live")}
    ></span>
    {stale ? "Last seen" : "Live"}
    <span class="ambrose-mono text-fg-muted">{age}</span>
</span>
