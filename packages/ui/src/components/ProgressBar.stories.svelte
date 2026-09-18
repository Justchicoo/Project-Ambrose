<!-- Project Ambrose by Imjustchico: Progress with its real numbers, and the indeterminate bar that is only allowed while something is genuinely running. -->
<script module lang="ts">
    import { defineMeta } from "@storybook/addon-svelte-csf";
    import ProgressBar from "./ProgressBar.svelte";

    const { Story } = defineMeta({
        title: "State/ProgressBar",
        component: ProgressBar,
        tags: ["autodocs"],
    });
</script>

<Story name="Determinate">
    {#snippet template()}
        <div class="flex w-full flex-col gap-16">
            <ProgressBar label="Extracting archives" value={64} detail="64 of 100 files" />
            <ProgressBar label="Disk in use" value={91} tone="wrong" detail="91 of 100 GB" />
            <ProgressBar label="Verified" value={100} tone="healthy" detail="Every file" />
        </div>
    {/snippet}
</Story>

<Story name="Indeterminate">
    {#snippet template()}
        <ProgressBar label="Rebuilding the type dump" indeterminate />
    {/snippet}
</Story>
