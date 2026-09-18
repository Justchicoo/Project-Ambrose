<!-- Project Ambrose by Imjustchico: A modal with a real title and description, a focus trap and an escape route, fading and scaling only when motion is on. -->
<script lang="ts">
    import type { Snippet } from "svelte";
    import { Dialog } from "bits-ui";
    import { classes } from "../internal/classes";
    import IconButton from "./IconButton.svelte";

    type Props = {
        open?: boolean;
        title: string;
        description?: string;
        closeLabel?: string;
        class?: string;
        trigger?: Snippet<[Record<string, unknown>]>;
        footer?: Snippet;
        children: Snippet;
    };

    let { open = $bindable(false), title, description, closeLabel = "Close", class: extra, trigger, footer, children }: Props = $props();
</script>

<Dialog.Root bind:open>
    {#if trigger}
        <Dialog.Trigger>
            {#snippet child({ props })}
                {@render trigger(props)}
            {/snippet}
        </Dialog.Trigger>
    {/if}
    <Dialog.Portal>
        <Dialog.Overlay class="ambrose-panel fixed inset-0 z-40 bg-surface-chrome/70" />
        <Dialog.Content
            style="max-width: var(--ambrose-size-dialog)"
            class={classes(
                "ambrose-panel fixed top-1/2 left-1/2 z-50 w-full -translate-x-1/2 -translate-y-1/2 rounded-card border border-edge-strong bg-surface-card",
                extra,
            )}
        >
            <header class="flex items-start justify-between gap-16 border-b border-edge-quiet px-20 py-14">
                <div class="flex flex-col gap-4">
                    <Dialog.Title class="font-display text-21 font-semibold text-fg-body">{title}</Dialog.Title>
                    {#if description}
                        <Dialog.Description class="text-13 text-fg-muted">{description}</Dialog.Description>
                    {/if}
                </div>
                <Dialog.Close>
                    {#snippet child({ props })}
                        <IconButton {...props} icon="x" label={closeLabel} />
                    {/snippet}
                </Dialog.Close>
            </header>
            <div class="flex flex-col gap-12 px-20 py-16">
                {@render children()}
            </div>
            {#if footer}
                <footer class="flex items-center justify-end gap-8 border-t border-edge-quiet px-20 py-14">
                    {@render footer()}
                </footer>
            {/if}
        </Dialog.Content>
    </Dialog.Portal>
</Dialog.Root>
