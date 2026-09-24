<!-- Project Ambrose by Imjustchico: A state as a colour and a word together, because the colour alone is not a state a person can read. -->
<script lang="ts">
    import { classes } from "../internal/classes";

    type State = "healthy" | "waiting" | "wrong" | "unknown";

    type Props = {
        state: State;
        word: string;
        live?: boolean;
        size?: "11" | "12" | "13" | "15";
        class?: string;
    };

    let { state, word, live = false, size = "13", class: extra }: Props = $props();

    const dots: Record<State, string> = {
        healthy: "bg-state-healthy",
        waiting: "bg-state-waiting",
        wrong: "bg-state-wrong",
        unknown: "bg-state-unknown",
    };
    const words: Record<State, string> = {
        healthy: "text-state-healthy",
        waiting: "text-state-waiting",
        wrong: "text-state-wrong",
        unknown: "text-state-unknown",
    };
    const sizes: Record<string, string> = {
        "11": "text-11",
        "12": "text-12",
        "13": "text-13",
        "15": "text-15",
    };
</script>

<span class={classes("inline-flex items-center gap-8", sizes[size], words[state], extra)}>
    <span class={classes("h-8 w-8 shrink-0 rounded-pill", dots[state], live ? "ambrose-live" : null)}></span>
    {word}
</span>
