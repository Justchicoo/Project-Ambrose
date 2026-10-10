<!-- Project Ambrose by Imjustchico: The command palette that Ctrl+K or Cmd+K opens from anywhere: type to jump to a page the viewer may use, open the overview of an app the panel reads, or switch between light and dark. -->
<script lang="ts">
    import * as Command from "$lib/components/ui/command/index.js";
    import { live } from "$lib/status.svelte.js";
    import { t } from "$lib/i18n.svelte.js";
    import { chooseTheme, type ThemeChoice } from "$lib/theme.svelte.js";
    import GaugeIcon from "@lucide/svelte/icons/gauge";
    import MonitorIcon from "@lucide/svelte/icons/monitor";
    import MoonIcon from "@lucide/svelte/icons/moon";
    import SunIcon from "@lucide/svelte/icons/sun";
    import type { Route } from "../routes";

    type Props = { open: boolean; pages: Route[] };
    let { open = $bindable(false), pages }: Props = $props();

    const looks: { choice: ThemeChoice; labelKey: string; icon: typeof SunIcon }[] = [
        { choice: "system", labelKey: "theme.system", icon: MonitorIcon },
        { choice: "light", labelKey: "theme.light", icon: SunIcon },
        { choice: "dark", labelKey: "theme.dark", icon: MoonIcon },
    ];

    $effect(() => {
        const toggle = (event: KeyboardEvent) => {
            if (event.key.toLowerCase() === "k" && (event.metaKey || event.ctrlKey)) {
                event.preventDefault();
                open = !open;
            }
        };
        document.addEventListener("keydown", toggle);
        return () => document.removeEventListener("keydown", toggle);
    });

    function run(action: () => void) {
        open = false;
        action();
    }
</script>

<Command.Dialog bind:open title={t("palette.title")} description={t("palette.description")}>
    <Command.Input placeholder={t("palette.placeholder")} />
    <Command.List>
        <Command.Empty>{t("palette.empty")}</Command.Empty>
        <Command.Group heading={t("palette.pages")}>
            {#each pages as page (page.path)}
                <Command.Item value={`page ${t(page.title)}`} onSelect={() => run(() => (window.location.hash = `#${page.path}`))}>
                    <page.icon />
                    <span>{t(page.title)}</span>
                </Command.Item>
            {/each}
        </Command.Group>
        {#if live.apps.length > 0}
            <Command.Separator />
            <Command.Group heading={t("palette.apps")}>
                {#each live.apps as app (app.name)}
                    <Command.Item
                        value={`${app.name} overview`}
                        keywords={[app.role, app.realm]}
                        onSelect={() => run(() => (window.location.hash = "#overview"))}
                    >
                        <GaugeIcon />
                        <span>{t("palette.openOverview", { app: app.name })}</span>
                    </Command.Item>
                {/each}
            </Command.Group>
        {/if}
        <Command.Separator />
        <Command.Group heading={t("palette.appearance")}>
            {#each looks as look (look.choice)}
                <Command.Item
                    value={`theme ${t(look.labelKey)}`}
                    keywords={["theme", "mode", "appearance"]}
                    onSelect={() => run(() => chooseTheme(look.choice))}
                >
                    <look.icon />
                    <span>{t(look.labelKey)}</span>
                </Command.Item>
            {/each}
        </Command.Group>
    </Command.List>
</Command.Dialog>
