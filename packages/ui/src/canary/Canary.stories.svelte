<!-- Project Ambrose by Imjustchico: A story with a deliberately unlabelled control, which the canary run asserts fails, so a gate that quietly stopped checking cannot pass unnoticed. -->
<script module lang="ts">
    import { defineMeta } from "@storybook/addon-svelte-csf";
    import Icon from "../components/Icon.svelte";

    const { Story } = defineMeta({
        title: "Canary/Unlabelled control",
    });
</script>

<Story name="An icon with no label">
    {#snippet template()}
        <button
            type="button"
            class="inline-flex min-h-44 min-w-44 items-center justify-center rounded-input bg-surface-sunken text-fg-body"
        >
            <Icon name="trash-2" />
        </button>
    {/snippet}
</Story>
