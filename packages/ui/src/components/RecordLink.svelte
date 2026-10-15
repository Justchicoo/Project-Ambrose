<!-- Project Ambrose by Imjustchico: A link that opens another record's page by its address. -->
<script lang="ts">
    import { classes } from "../internal/classes";
    import ArrowUpRightIcon from "@lucide/svelte/icons/arrow-up-right";

    type Props = {
        href: string;
        label: string;
        title?: string;
        class?: string;
    };

    let { href, label, title, class: extra }: Props = $props();
</script>

<a
    {href}
    {title}
    class={classes(
        "inline-flex items-baseline gap-4 font-mono text-13 text-fill-action underline decoration-fill-action/40 underline-offset-2 hover:decoration-fill-action",
        extra,
    )}
>
    <span>{label}</span>
    <ArrowUpRightIcon class="size-12 self-center" aria-hidden="true" />
</a>
