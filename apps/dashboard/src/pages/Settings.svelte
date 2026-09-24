<!-- Project Ambrose by Imjustchico: The panel settings editor: general, mail and security values grouped like the server model, locked listener-owned values identified by their layer, secrets masked with an explicit clear action, and changes submitted as one audited batch. -->
<script lang="ts">
    import * as Card from "$lib/components/ui/card/index.js";
    import * as Tabs from "$lib/components/ui/tabs/index.js";
    import { Button } from "$lib/components/ui/button/index.js";
    import { Input } from "$lib/components/ui/input/index.js";
    import { Label } from "$lib/components/ui/label/index.js";
    import { ApiError } from "$lib/api.svelte.js";
    import { panelSettings, testPanelMail, updatePanelSettings } from "$lib/supervision.svelte.js";
    import type { InferOutput } from "valibot";
    import { PanelSettingsAnswer } from "$lib/schemas.js";
    import MailIcon from "@lucide/svelte/icons/mail";
    import SaveIcon from "@lucide/svelte/icons/save";
    import PageHeader from "../components/PageHeader.svelte";
    import StatusBadge from "../components/StatusBadge.svelte";

    type Setting = InferOutput<typeof PanelSettingsAnswer>["settings"][number];
    let answer = $state<InferOutput<typeof PanelSettingsAnswer> | null>(null);
    let values = $state<Record<string, string>>({});
    let failure = $state("");
    let notice = $state("");
    let busy = $state(false);

    $effect(() => {
        void (async () => {
            try {
                answer = await panelSettings();
                values = Object.fromEntries(answer.settings.map((setting) => [setting.key, setting.value]));
            } catch (problem) {
                failure = problem instanceof ApiError ? problem.message : "The panel settings could not be read";
            }
        })();
    });

    const groups = [
        { id: "general", label: "General", description: "Identity, locale and retention defaults." },
        { id: "mail", label: "Mail", description: "SMTP delivery for verification and alerts." },
        { id: "security", label: "Security", description: "Sign-in, relay and captcha controls." },
    ] as const;

    function shown(group: Setting["group"]) {
        return answer?.settings.filter((setting) => setting.group === group) ?? [];
    }

    async function save() {
        busy = true;
        failure = "";
        notice = "";
        try {
            answer = await updatePanelSettings(values);
            notice = "Settings saved and audited.";
        } catch (problem) {
            failure = problem instanceof ApiError ? problem.message : "The panel settings could not be saved";
        } finally {
            busy = false;
        }
    }

    async function sendTest() {
        try {
            await testPanelMail();
            notice = "A test message was sent to your signed-in address.";
        } catch (problem) {
            failure = problem instanceof ApiError ? problem.message : "The mail test failed";
        }
    }
</script>

<PageHeader
    title="Panel settings"
    description="Configure the panel itself. Changes are typed, validated and written to the activity log."
>
    {#snippet actions()}
        <Button onclick={() => void save()} disabled={busy || answer === null}><SaveIcon />{busy ? "Saving…" : "Save changes"}</Button>
    {/snippet}
</PageHeader>

{#if failure}<p class="rounded-md border border-destructive/30 bg-destructive/5 p-3 text-sm text-destructive" role="alert">{failure}</p>{/if}
{#if notice}<p class="rounded-md border border-healthy/30 bg-healthy/5 p-3 text-sm text-healthy" role="status">{notice}</p>{/if}

<Tabs.Root value="general">
    <Tabs.List>
        {#each groups as group (group.id)}<Tabs.Trigger value={group.id}>{group.label}</Tabs.Trigger>{/each}
    </Tabs.List>
    {#each groups as group (group.id)}
        <Tabs.Content value={group.id} class="mt-4 space-y-4">
            <Card.Root class="shadow-xs">
                <Card.Header><Card.Title>{group.label}</Card.Title><Card.Description>{group.description}</Card.Description></Card.Header>
                <Card.Content class="grid gap-5 md:grid-cols-2">
                    {#each shown(group.id) as setting (setting.key)}
                        <div class="space-y-2">
                            <Label for={setting.key}>{setting.key}</Label>
                            <div class="flex items-center gap-2">
                                <Input
                                    id={setting.key}
                                    type={setting.secret ? "password" : "text"}
                                    value={values[setting.key] === "***" ? "" : values[setting.key]}
                                    placeholder={setting.secret ? "Unchanged" : setting.default || "Not set"}
                                    disabled={setting.locked}
                                    onchange={(event) => (values[setting.key] = event.currentTarget.value)}
                                />
                                {#if setting.secret && !setting.locked}
                                    <Button variant="ghost" size="sm" onclick={() => (values[setting.key] = "")}>Clear</Button>
                                {/if}
                                {#if setting.locked}
                                    <StatusBadge tone="unknown">Locked · {setting.layer}</StatusBadge>
                                {/if}
                            </div>
                            {#if setting.secret}<p class="text-xs text-muted-foreground">Stored secret is never returned. Leave blank to keep it.</p>{/if}
                        </div>
                    {/each}
                </Card.Content>
                {#if group.id === "mail"}
                    <Card.Footer><Button variant="outline" onclick={() => void sendTest()}><MailIcon />Send test to me</Button></Card.Footer>
                {/if}
            </Card.Root>
        </Tabs.Content>
    {/each}
</Tabs.Root>
