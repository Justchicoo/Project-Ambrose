<!-- Project Ambrose by Imjustchico: Every vendored icon, and the difference between one that decorates and one that carries a meaning. -->
<script module lang="ts">
    import { defineMeta } from "@storybook/addon-svelte-csf";
    import Icon from "./Icon.svelte";
    import { iconNames } from "../icons/icons";

    const { Story } = defineMeta({
        title: "Foundations/Icon",
        component: Icon,
        tags: ["autodocs"],
    });
</script>

<Story name="The set in use">
    {#snippet template()}
        <ul class="grid list-none grid-cols-6 gap-12 p-0">
            {#each iconNames as name (name)}
                <li class="flex flex-col items-center gap-6 rounded-input bg-surface-card p-10">
                    <Icon {name} size="21" />
                    <span class="text-11 text-fg-faint">{name}</span>
                </li>
            {/each}
        </ul>
    {/snippet}
</Story>

<Story name="Labelled and decorative">
    {#snippet template()}
        <div class="flex items-center gap-16">
            <Icon name="circle-check" label="Verified" class="text-state-healthy" />
            <span class="flex items-center gap-6 text-13 text-fg-body">
                <Icon name="server" />
                Ravenwood
            </span>
        </div>
    {/snippet}
</Story>
