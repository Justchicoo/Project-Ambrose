<!-- Project Ambrose by Imjustchico: The confirmation a destructive action has to pass, which states what will happen, names the thing by its own name and puts the one dangerous button last. -->
<script lang="ts">
    import { AlertDialog } from "bits-ui";
    import Button from "./Button.svelte";

    type Props = {
        open?: boolean;
        title: string;
        description: string;
        confirmLabel?: string;
        cancelLabel?: string;
        subject?: string;
        onconfirm?: () => void;
    };

    let {
        open = $bindable(false),
        title,
        description,
        confirmLabel = "Confirm",
        cancelLabel = "Cancel",
        subject,
        onconfirm,
    }: Props = $props();

    function confirm() {
        open = false;
        onconfirm?.();
    }
</script>

<AlertDialog.Root bind:open>
    <AlertDialog.Portal>
        <AlertDialog.Overlay class="ambrose-panel fixed inset-0 z-40 bg-surface-chrome/70" />
        <AlertDialog.Content
            style="max-width: var(--ambrose-size-dialog)"
            class="ambrose-panel fixed top-1/2 left-1/2 z-50 w-full -translate-x-1/2 -translate-y-1/2 rounded-card border border-state-wrong bg-surface-card"
        >
            <div class="flex flex-col gap-10 px-20 py-16">
                <AlertDialog.Title class="font-display text-21 font-semibold text-fg-body">{title}</AlertDialog.Title>
                <AlertDialog.Description class="text-13 text-fg-muted">{description}</AlertDialog.Description>
                {#if subject}
                    <p class="ambrose-mono rounded-input border border-edge-strong bg-surface-sunken px-12 py-10 text-13 text-fg-body">
                        {subject}
                    </p>
                {/if}
            </div>
            <footer class="flex items-center justify-end gap-8 border-t border-edge-quiet px-20 py-14">
                <AlertDialog.Cancel>
                    {#snippet child({ props })}
                        <Button {...props} variant="quiet">{cancelLabel}</Button>
                    {/snippet}
                </AlertDialog.Cancel>
                <Button variant="danger" onclick={confirm}>{confirmLabel}</Button>
            </footer>
        </AlertDialog.Content>
    </AlertDialog.Portal>
</AlertDialog.Root>
