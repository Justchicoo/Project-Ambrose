<!-- Project Ambrose by Imjustchico: The panel settings editor: general, mail and security values grouped like the server model, locked listener-owned values identified by their layer, secrets masked with an explicit clear action, and changes submitted as one audited batch. The mail group sends a test mail through the saved settings to the signed-in operator alone and shows who it reached, or the mail server's own refusal. The operator's own language is chosen here, remembered in this browser, with the installation default from Panel.Locale. -->
<script lang="ts">
    import * as Card from "$lib/components/ui/card/index.js";
    import * as Tabs from "$lib/components/ui/tabs/index.js";
    import * as Select from "$lib/components/ui/select/index.js";
    import { Button } from "$lib/components/ui/button/index.js";
    import { Input } from "$lib/components/ui/input/index.js";
    import { Label } from "$lib/components/ui/label/index.js";
    import { ApiError } from "$lib/api.svelte.js";
    import { panelSettings, testPanelMail, updatePanelSettings } from "$lib/supervision.svelte.js";
    import { availableLocales, chooseLocale, i18n, localeDisplayName, setInstallationDefault, t } from "$lib/i18n.svelte.js";
    import type { InferOutput } from "valibot";
    import { PanelSettingsAnswer } from "$lib/schemas.js";
    import SaveIcon from "@lucide/svelte/icons/save";
    import SendIcon from "@lucide/svelte/icons/send";
    import PageHeader from "../components/PageHeader.svelte";
    import StatusBadge from "../components/StatusBadge.svelte";

    type Setting = InferOutput<typeof PanelSettingsAnswer>["settings"][number];
    let answer = $state<InferOutput<typeof PanelSettingsAnswer> | null>(null);
    let values = $state<Record<string, string>>({});
    let failure = $state("");
    let notice = $state("");
    let busy = $state(false);
    let mailing = $state(false);

    $effect(() => {
        void (async () => {
            try {
                answer = await panelSettings();
                values = Object.fromEntries(answer.settings.map((setting) => [setting.key, setting.value]));
                const locale = answer.settings.find((setting) => setting.key === "Panel.Locale");
                if (locale?.value) setInstallationDefault(locale.value);
            } catch (problem) {
                failure = problem instanceof ApiError ? problem.message : t("settings.readError");
            }
        })();
    });

    const groups = [
        { id: "general", labelKey: "settings.tab.general", descriptionKey: "settings.tab.general.description" },
        { id: "mail", labelKey: "settings.tab.mail", descriptionKey: "settings.tab.mail.description" },
        { id: "security", labelKey: "settings.tab.security", descriptionKey: "settings.tab.security.description" },
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
            notice = t("settings.saved");
        } catch (problem) {
            failure = problem instanceof ApiError ? problem.message : t("settings.saveError");
        } finally {
            busy = false;
        }
    }

    async function testMail() {
        mailing = true;
        failure = "";
        notice = "";
        try {
            const sent = await testPanelMail();
            notice = t("settings.sentTo", { to: sent.to });
        } catch (problem) {
            failure = problem instanceof ApiError ? problem.message : t("settings.sendError");
        } finally {
            mailing = false;
        }
    }
</script>

<PageHeader title={t("settings.title")} description={t("settings.description")}>
    {#snippet actions()}
        <Button onclick={() => void save()} disabled={busy || answer === null}
            ><SaveIcon />{busy ? t("settings.saving") : t("settings.save")}</Button
        >
    {/snippet}
</PageHeader>

{#if failure}
    <p class="rounded-md border border-destructive/30 bg-destructive/5 p-3 text-sm text-destructive" role="alert">{failure}</p>
{/if}
{#if notice}
    <p class="rounded-md border border-healthy/30 bg-healthy/5 p-3 text-sm text-healthy" role="status">{notice}</p>
{/if}

<Card.Root class="mb-4 shadow-xs">
    <Card.Header>
        <Card.Title>{t("settings.locale.label")}</Card.Title>
        <Card.Description>{t("settings.locale.description")}</Card.Description>
    </Card.Header>
    <Card.Content>
        <div class="max-w-xs space-y-2">
            <Label for="dashboard-locale">{t("settings.locale.label")}</Label>
            <Select.Root type="single" value={i18n.locale} onValueChange={(value) => value && chooseLocale(value)}>
                <Select.Trigger id="dashboard-locale" class="w-44" aria-label={t("settings.locale.label")}>
                    {localeDisplayName(i18n.locale)}
                </Select.Trigger>
                <Select.Content>
                    {#each availableLocales as option (option.tag)}
                        <Select.Item value={option.tag}>{localeDisplayName(option.tag)}</Select.Item>
                    {/each}
                </Select.Content>
            </Select.Root>
        </div>
    </Card.Content>
</Card.Root>

<Tabs.Root value="general">
    <Tabs.List>
        {#each groups as group (group.id)}
            <Tabs.Trigger value={group.id}>{t(group.labelKey)}</Tabs.Trigger>
        {/each}
    </Tabs.List>
    {#each groups as group (group.id)}
        <Tabs.Content value={group.id} class="mt-4 space-y-4">
            <Card.Root class="shadow-xs">
                <Card.Header>
                    <Card.Title>{t(group.labelKey)}</Card.Title>
                    <Card.Description>{t(group.descriptionKey)}</Card.Description>
                </Card.Header>
                <Card.Content class="grid gap-5 md:grid-cols-2">
                    {#each shown(group.id) as setting (setting.key)}
                        <div class="space-y-2">
                            <Label for={setting.key}>{setting.key}</Label>
                            <div class="flex items-center gap-2">
                                <Input
                                    id={setting.key}
                                    type={setting.secret ? "password" : "text"}
                                    value={values[setting.key] === "***" ? "" : values[setting.key]}
                                    placeholder={setting.secret ? t("settings.unchanged") : setting.default || t("settings.notSet")}
                                    disabled={setting.locked}
                                    onchange={(event) => (values[setting.key] = event.currentTarget.value)}
                                />
                                {#if setting.secret && !setting.locked}
                                    <Button variant="ghost" size="sm" onclick={() => (values[setting.key] = "")}
                                        >{t("settings.clear")}</Button
                                    >
                                {/if}
                                {#if setting.locked}
                                    <StatusBadge tone="unknown">{t("settings.locked")} · {setting.layer}</StatusBadge>
                                {/if}
                            </div>
                            {#if setting.secret}
                                <p class="text-xs text-muted-foreground">{t("settings.secretHint")}</p>
                            {/if}
                        </div>
                    {/each}
                </Card.Content>
                {#if group.id === "mail"}
                    <Card.Footer class="flex flex-wrap items-center gap-3">
                        <Button variant="outline" onclick={() => void testMail()} disabled={mailing || answer === null}>
                            <SendIcon />{mailing ? t("settings.sending") : t("settings.sendTest")}
                        </Button>
                        <p class="text-xs text-muted-foreground">{t("settings.mailHint")}</p>
                    </Card.Footer>
                {/if}
            </Card.Root>
        </Tabs.Content>
    {/each}
</Tabs.Root>
