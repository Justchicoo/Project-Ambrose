<!-- Project Ambrose by Imjustchico: The client data page: the install the servers read, its revision, the type dump built from it and whether it is current, with the button that rebuilds it. -->
<script lang="ts">
    import * as Card from "$lib/components/ui/card/index.js";
    import { Button } from "$lib/components/ui/button/index.js";
    import FolderOpenIcon from "@lucide/svelte/icons/folder-open";
    import RefreshCwIcon from "@lucide/svelte/icons/refresh-cw";
    import PageHeader from "../components/PageHeader.svelte";
    import StatusBadge from "../components/StatusBadge.svelte";
</script>

<PageHeader title="Client data" description="The Wizard101 install the servers read, and what they built from it." />

<div class="grid items-start gap-4 lg:grid-cols-2">
    <Card.Root class="shadow-xs">
        <Card.Header>
            <Card.Title>Install</Card.Title>
            <Card.Description>Found automatically on the first start.</Card.Description>
            <Card.Action><StatusBadge tone="healthy">Found</StatusBadge></Card.Action>
        </Card.Header>
        <Card.Content>
            <dl class="space-y-3 text-sm">
                <div>
                    <dt class="text-xs text-muted-foreground">Folder</dt>
                    <dd class="font-mono text-xs break-all">C:\ProgramData\KingsIsle Entertainment\Wizard101</dd>
                </div>
                <div>
                    <dt class="text-xs text-muted-foreground">Revision</dt>
                    <dd class="font-mono">r806919 · Wizard 1.610</dd>
                </div>
            </dl>
        </Card.Content>
        <Card.Footer><Button variant="outline" size="sm"><FolderOpenIcon />Choose another install</Button></Card.Footer>
    </Card.Root>
    <Card.Root class="border-waiting/40 shadow-xs">
        <Card.Header>
            <Card.Title>Type dump</Card.Title>
            <Card.Description>The class list the servers decode game data with.</Card.Description>
            <Card.Action><StatusBadge tone="waiting">Out of date</StatusBadge></Card.Action>
        </Card.Header>
        <Card.Content>
            <dl class="space-y-3 text-sm">
                <div>
                    <dt class="text-xs text-muted-foreground">Built from</dt>
                    <dd class="font-mono">r801440</dd>
                </div>
                <div>
                    <dt class="text-xs text-muted-foreground">Why it matters</dt>
                    <dd>The install is newer, so some game data may not decode until the dump is rebuilt.</dd>
                </div>
            </dl>
        </Card.Content>
        <Card.Footer><Button size="sm"><RefreshCwIcon />Rebuild from the install</Button></Card.Footer>
    </Card.Root>
</div>
