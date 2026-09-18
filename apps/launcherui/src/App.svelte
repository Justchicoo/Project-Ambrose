<!-- Project Ambrose by Imjustchico: The launcher window as it stands before 3.26 fills it: one gold Play, the three state cards and the guarantee line at the foot, all from the shared components. -->
<script lang="ts">
    import { AppShell, Button, Card, Heading, StateDot, StepList } from "@ambrose/ui";

    const steps = [
        { id: "install", label: "Find the installation", state: "unknown" as const, word: "Not looked yet" },
        { id: "dump", label: "Build the type dump", state: "unknown" as const, word: "Not looked yet" },
        { id: "server", label: "Reach the server", state: "unknown" as const, word: "Not looked yet" },
    ];
</script>

<AppShell product="Ambrose">
    <div class="flex flex-col gap-20">
        <Heading level={1}>Ready when the server is</Heading>
        <div class="grid grid-cols-3 gap-16">
            <Card title="Server">
                <StateDot state="unknown" word="Not reached" />
            </Card>
            <Card title="Client">
                <StateDot state="unknown" word="Not checked" />
            </Card>
            <Card title="Account">
                <StateDot state="unknown" word="Not signed in" />
            </Card>
        </div>
        <Card title="First run">
            <StepList label="First run" {steps} />
        </Card>
        <div class="flex items-center justify-between gap-16">
            <p class="text-12 text-fg-faint">
                Ambrose reads your own installation and writes nothing into it. It never starts KingsIsle's launcher or patcher.
            </p>
            <Button variant="action" size="wide" icon="play">Play</Button>
        </div>
    </div>
</AppShell>
