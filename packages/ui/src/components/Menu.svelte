<!-- Project Ambrose by Imjustchico: The row of extra actions behind one trigger, where a destructive entry is marked and an entry the caller may not run is never listed at all. -->
<script lang="ts">
    import { DropdownMenu } from "bits-ui";
    import Icon from "./Icon.svelte";
    import IconButton from "./IconButton.svelte";
    import type { IconName } from "../icons/icons";

    type Entry = {
        id: string;
        label: string;
        icon?: IconName;
        danger?: boolean;
        disabled?: boolean;
    };

    type Props = {
        label: string;
        items: Entry[];
        trigger?: IconName;
        onchoose?: (id: string) => void;
    };

    let { label, items, trigger = "list", onchoose }: Props = $props();
</script>

<DropdownMenu.Root>
    <DropdownMenu.Trigger>
        {#snippet child({ props })}
            <IconButton {...props} icon={trigger} {label} />
        {/snippet}
    </DropdownMenu.Trigger>
    <DropdownMenu.Portal>
        <DropdownMenu.Content sideOffset={6} class="ambrose-panel z-50 min-w-44 rounded-card border border-edge-strong bg-surface-card p-6">
            {#each items as item (item.id)}
                <DropdownMenu.Item
                    disabled={item.disabled}
                    onSelect={() => onchoose?.(item.id)}
                    class="ambrose-flip flex min-h-44 cursor-default items-center gap-10 rounded-control px-12 text-15 data-highlighted:bg-surface-sunken data-disabled:opacity-50 {item.danger
                        ? 'text-state-wrong'
                        : 'text-fg-body'}"
                >
                    {#if item.icon}
                        <Icon name={item.icon} size="15" />
                    {/if}
                    {item.label}
                </DropdownMenu.Item>
            {/each}
        </DropdownMenu.Content>
    </DropdownMenu.Portal>
</DropdownMenu.Root>
