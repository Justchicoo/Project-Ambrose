<!-- Project Ambrose by Imjustchico: Where toasts appear, styled through the tokens, with error toasts that never dismiss themselves because a toast is never the only record of an event. -->
<script lang="ts">
    import { Toaster } from "svelte-sonner";
    import { motion } from "../motion/motion.svelte";

    type Props = {
        position?: "top-right" | "bottom-right" | "bottom-center";
    };

    let { position = "bottom-right" }: Props = $props();
</script>

<Toaster
    {position}
    closeButton
    duration={motion.off ? 8000 : 5000}
    toastOptions={{
        unstyled: false,
        classes: {
            toast: "ambrose-toast",
            error: "ambrose-toast-error",
            success: "ambrose-toast-healthy",
            title: "ambrose-toast-title",
            description: "ambrose-toast-description",
        },
    }}
/>

<style>
    :global(.ambrose-toast) {
        background-color: var(--color-surface-card);
        border: var(--ambrose-size-border) solid var(--color-edge-strong);
        border-radius: var(--radius-card);
        color: var(--color-fg-body);
        font-family: var(--font-interface);
        font-size: var(--text-13);
    }

    :global(.ambrose-toast-error) {
        border-color: var(--color-state-wrong);
    }

    :global(.ambrose-toast-healthy) {
        border-color: var(--color-state-healthy);
    }

    :global(.ambrose-toast-title) {
        color: var(--color-fg-body);
    }

    :global(.ambrose-toast-description) {
        color: var(--color-fg-muted);
    }
</style>
