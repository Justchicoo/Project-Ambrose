<!-- Project Ambrose by Imjustchico: The side bar's grouped links, with and without a page marked as the one you are on. -->
<script module lang="ts">
    import { defineMeta } from "@storybook/addon-svelte-csf";
    import SideNav from "./SideNav.svelte";

    const { Story } = defineMeta({
        title: "Shell/SideNav",
        component: SideNav,
        tags: ["autodocs"],
    });

    const groups = [
        {
            heading: "This machine",
            entries: [
                { href: "#overview", label: "Overview", icon: "gauge" as const },
                { href: "#servers", label: "Servers", icon: "server" as const },
                { href: "#files", label: "Files", icon: "folder" as const },
            ],
        },
        {
            heading: "Realm",
            entries: [
                { href: "#players", label: "Players", icon: "users" as const },
                { href: "#world", label: "World data", icon: "database" as const },
            ],
        },
    ];
</script>

<Story name="On a page">
    {#snippet template()}
        <div class="w-full bg-surface-chrome" style="max-width: var(--ambrose-size-side-bar)">
            <SideNav label="Panel sections" {groups} current="#servers" />
        </div>
    {/snippet}
</Story>

<Story name="Nowhere in particular">
    {#snippet template()}
        <div class="w-full bg-surface-chrome" style="max-width: var(--ambrose-size-side-bar)">
            <SideNav label="Panel sections" {groups} />
        </div>
    {/snippet}
</Story>
