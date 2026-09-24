<!-- Project Ambrose by Imjustchico: Where toasts appear, and the rule that a toast is never the only record of an event. -->
<script module lang="ts">
    import { defineMeta } from "@storybook/addon-svelte-csf";
    import { toast } from "svelte-sonner";
    import Toaster from "./Toaster.svelte";
    import Button from "./Button.svelte";

    const { Story } = defineMeta({
        title: "State/Toaster",
        component: Toaster,
        tags: ["autodocs"],
    });
</script>

<Story name="Raising one">
    {#snippet template()}
        <div class="flex items-center gap-12">
            <Toaster />
            <Button variant="quiet" onclick={() => toast.success("Backup finished", { description: "Also in the notification centre." })}>
                Finish a backup
            </Button>
            <Button
                variant="quiet"
                onclick={() => toast.error("Backup refused", { description: "Also in the audit log.", duration: Number.POSITIVE_INFINITY })}
            >
                Refuse a backup
            </Button>
        </div>
    {/snippet}
</Story>
