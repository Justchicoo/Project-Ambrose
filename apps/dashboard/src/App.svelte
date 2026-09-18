<!-- Project Ambrose by Imjustchico: The panel shell as it stands before 17.06 fills it: the frame, the side bar and a card saying what this build already proves. -->
<script lang="ts">
    import { AppShell, Card, Heading, SideNav, StateDot, StatTile, Button } from "@ambrose/ui";

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

<AppShell product="Ambrose">
    {#snippet side()}
        <SideNav label="Panel sections" {groups} current="#overview" />
    {/snippet}
    {#snippet barEnd()}
        <Button variant="action" icon="play">Start all</Button>
    {/snippet}
    <div class="flex flex-col gap-20">
        <Heading level={1}>Overview</Heading>
        <div class="grid grid-cols-3 gap-16">
            <StatTile label="Servers running" value="0" state="unknown" word="Not connected" />
            <StatTile label="Players online" value="0" state="unknown" word="Not connected" />
            <StatTile label="Panel" value="17.73" state="healthy" word="Design system" />
        </div>
        <Card title="What this build proves">
            <p class="text-13 text-fg-muted">
                The panel, the launcher window and the terminal read one set of tokens. This page loads them and nothing else yet.
            </p>
            <StateDot state="waiting" word="Waiting for the admin API in 17.06" />
        </Card>
    </div>
</AppShell>
