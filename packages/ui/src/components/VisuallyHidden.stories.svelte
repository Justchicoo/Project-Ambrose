<!-- Project Ambrose by Imjustchico: Text only a screen reader reads, which is how an icon-only control carries its label. -->
<script module lang="ts">
    import { defineMeta } from "@storybook/addon-svelte-csf";
    import VisuallyHidden from "./VisuallyHidden.svelte";
    import Icon from "./Icon.svelte";

    const { Story } = defineMeta({
        title: "Foundations/VisuallyHidden",
        component: VisuallyHidden,
        tags: ["autodocs"],
    });
</script>

<Story name="Inside a control">
    {#snippet template()}
        <button
            type="button"
            class="inline-flex min-h-44 min-w-44 items-center justify-center rounded-input bg-surface-sunken text-fg-body"
        >
            <Icon name="copy" />
            <VisuallyHidden>Copy the connection string</VisuallyHidden>
        </button>
    {/snippet}
</Story>
