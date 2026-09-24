<!-- Project Ambrose by Imjustchico: The frame both surfaces share, with and without a side bar. -->
<script module lang="ts">
    import { defineMeta } from "@storybook/addon-svelte-csf";
    import AppShell from "./AppShell.svelte";
    import SideNav from "./SideNav.svelte";
    import Button from "./Button.svelte";
    import Heading from "./Heading.svelte";

    const { Story } = defineMeta({
        title: "Shell/AppShell",
        component: AppShell,
        tags: ["autodocs"],
    });

    const groups = [
        {
            heading: "This machine",
            entries: [
                { href: "#overview", label: "Overview", icon: "gauge" as const },
                { href: "#servers", label: "Servers", icon: "server" as const },
                { href: "#console", label: "Console", icon: "terminal" as const },
            ],
        },
        {
            heading: "Panel",
            entries: [
                { href: "#users", label: "Users", icon: "users" as const },
                { href: "#settings", label: "Settings", icon: "settings" as const },
            ],
        },
    ];
</script>

<Story name="The panel">
    {#snippet template()}
        <AppShell product="Ambrose">
            {#snippet side()}
                <SideNav label="Panel sections" {groups} current="#overview" />
            {/snippet}
            {#snippet barEnd()}
                <Button variant="action" icon="play">Start all</Button>
            {/snippet}
            <Heading level={1}>Overview</Heading>
        </AppShell>
    {/snippet}
</Story>

<Story name="The launcher window">
    {#snippet template()}
        <AppShell product="Ambrose Launcher">
            <Heading level={1}>Ready to play</Heading>
        </AppShell>
    {/snippet}
</Story>
