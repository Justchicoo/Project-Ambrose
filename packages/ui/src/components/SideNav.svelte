<!-- Project Ambrose by Imjustchico: The side bar's grouped links, where the page you are on is marked by aria-current as well as by its gold edge. -->
<script lang="ts">
    import { classes } from "../internal/classes";
    import Icon from "./Icon.svelte";
    import type { IconName } from "../icons/icons";

    type Entry = { href: string; label: string; icon?: IconName };
    type Group = { heading?: string; entries: Entry[] };

    type Props = {
        label: string;
        groups: Group[];
        current?: string;
        class?: string;
    };

    let { label, groups, current, class: extra }: Props = $props();
</script>

<nav aria-label={label} class={classes("flex flex-col gap-20 p-12", extra)}>
    {#each groups as group, index (group.heading ?? index)}
        <div class="flex flex-col gap-4">
            {#if group.heading}
                <span class="ambrose-label px-12 py-4">{group.heading}</span>
            {/if}
            <ul class="flex list-none flex-col gap-2 p-0">
                {#each group.entries as entry (entry.href)}
                    <li>
                        <a
                            href={entry.href}
                            aria-current={entry.href === current ? "page" : undefined}
                            class="ambrose-hover flex min-h-44 items-center gap-10 rounded-input border-l-2 border-transparent px-12 text-15 text-fg-muted hover:text-fg-body aria-[current=page]:border-action aria-[current=page]:bg-surface-sunken aria-[current=page]:text-fg-body"
                        >
                            {#if entry.icon}
                                <Icon name={entry.icon} size="15" />
                            {/if}
                            {entry.label}
                        </a>
                    </li>
                {/each}
            </ul>
        </div>
    {/each}
</nav>
