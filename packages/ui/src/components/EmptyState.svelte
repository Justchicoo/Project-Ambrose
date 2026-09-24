<!-- Project Ambrose by Imjustchico: What a region says when it holds nothing: why it is empty and the one thing to do about it. -->
<script lang="ts">
    import type { Snippet } from "svelte";
    import { classes } from "../internal/classes";
    import Icon from "./Icon.svelte";
    import type { IconName } from "../icons/icons";

    type Props = {
        title: string;
        detail?: string;
        icon?: IconName;
        class?: string;
        action?: Snippet;
    };

    let { title, detail, icon, class: extra, action }: Props = $props();
</script>

<div
    class={classes(
        "flex flex-col items-center gap-10 rounded-card border border-edge-quiet bg-surface-sunken px-20 py-40 text-center",
        extra,
    )}
>
    {#if icon}
        <Icon name={icon} size="26" class="text-fg-faint" />
    {/if}
    <p class="text-15 text-fg-body">{title}</p>
    {#if detail}
        <p class="text-13 text-fg-muted">{detail}</p>
    {/if}
    {#if action}
        <div class="pt-6">{@render action()}</div>
    {/if}
</div>
