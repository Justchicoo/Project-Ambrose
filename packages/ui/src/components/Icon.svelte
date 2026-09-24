<!-- Project Ambrose by Imjustchico: One of the vendored icons by name, hidden from a screen reader unless it is given a label of its own, and never the only thing that carries a meaning. -->
<script lang="ts">
    import { classes } from "../internal/classes";
    import { icons, type IconName } from "../icons/icons";

    type Props = {
        name: IconName;
        label?: string;
        size?: "13" | "15" | "17" | "21" | "26";
        class?: string;
    };

    let { name, label, size = "17", class: extra }: Props = $props();

    const sizes: Record<string, string> = {
        "13": "text-13",
        "15": "text-15",
        "17": "text-17",
        "21": "text-21",
        "26": "text-26",
    };
    const Glyph = $derived(icons[name]);
</script>

<span
    class={classes("inline-flex shrink-0 items-center justify-center", sizes[size], extra)}
    role={label ? "img" : "presentation"}
    aria-label={label}
    aria-hidden={label ? undefined : "true"}
>
    <Glyph />
</span>
