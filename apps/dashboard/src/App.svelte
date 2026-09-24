<!-- Project Ambrose by Imjustchico: The panel frame: it asks the server that served it whether this browser is signed in, shows the sign-in page when it is not and a plain notice when the server cannot be reached, and once signed in draws a skip link that moves focus to the page without touching the address, the shadcn-svelte sidebar from the route table, a top bar with the breadcrumb, the command palette's search button and the connection's state, a bar that says when the server stopped answering with a countdown and a retry, the user's menu with the light and dark choice and signing out, and the page the address names, which is the page itself, the milestone that brings it, or the access-denied page. -->
<script lang="ts">
    import * as Breadcrumb from "$lib/components/ui/breadcrumb/index.js";
    import * as DropdownMenu from "$lib/components/ui/dropdown-menu/index.js";
    import * as Kbd from "$lib/components/ui/kbd/index.js";
    import * as Sidebar from "$lib/components/ui/sidebar/index.js";
    import { Button } from "$lib/components/ui/button/index.js";
    import { Separator } from "$lib/components/ui/separator/index.js";
    import { Skeleton } from "$lib/components/ui/skeleton/index.js";
    import { Toaster } from "$lib/components/ui/sonner/index.js";
    import { probeSession, session, signOut } from "$lib/api.svelte.js";
    import { formatAge } from "$lib/format.js";
    import { isStale, live, retryNow, stop, watch } from "$lib/status.svelte.js";
    import { chooseTheme, theme, type ThemeChoice } from "$lib/theme.svelte.js";
    import ChevronsUpDownIcon from "@lucide/svelte/icons/chevrons-up-down";
    import KeyRoundIcon from "@lucide/svelte/icons/key-round";
    import LogOutIcon from "@lucide/svelte/icons/log-out";
    import RotateCcwIcon from "@lucide/svelte/icons/rotate-ccw";
    import SearchIcon from "@lucide/svelte/icons/search";
    import { onMount } from "svelte";
    import { toast } from "svelte-sonner";
    import CommandPalette from "./components/CommandPalette.svelte";
    import StatusBadge from "./components/StatusBadge.svelte";
    import { everything, navigation, resolve } from "./routes";
    import Denied from "./pages/Denied.svelte";
    import SignIn from "./pages/SignIn.svelte";
    import Unavailable from "./pages/Unavailable.svelte";

    const granted = everything;

    function readPath(): string {
        const path = window.location.hash.replace(/^#\/?/, "");
        return path === "" ? "overview" : path;
    }

    let path = $state(readPath());
    let paletteOpen = $state(false);
    const onMac = /Mac|iPhone|iPad/.test(navigator.userAgent);

    $effect(() => {
        const follow = () => (path = readPath());
        window.addEventListener("hashchange", follow);
        return () => window.removeEventListener("hashchange", follow);
    });

    onMount(() => {
        void probeSession();
    });

    $effect(() => {
        if (session.state !== "signed-in") return;
        watch();
        return () => stop();
    });

    const shown = $derived(resolve(path, granted));
    const title = $derived(shown.kind === "missing" ? "Not found" : shown.route.title);
    const reachable = navigation(granted);
    const groups = (["Servers", "Game", "Panel"] as const).map((heading) => ({
        heading,
        entries: reachable.filter((entry) => entry.group === heading),
    }));
    const stale = $derived(live.receivedAt !== 0 && isStale(live.receivedAt, live.now));
    const countdown = $derived(Math.max(0, Math.ceil((live.retryAt - live.now) / 1000)));
    const connection = $derived.by((): { tone: "healthy" | "waiting" | "unknown"; word: string } => {
        if (live.connection === "reconnecting") return { tone: "waiting", word: "Reconnecting" };
        if (live.receivedAt === 0) return { tone: "unknown", word: "Connecting" };
        return stale ? { tone: "unknown", word: "Stale" } : { tone: "healthy", word: "Live" };
    });

    async function leave() {
        try {
            await signOut();
        } catch {
            toast.error("Signing out failed; the session ends by itself when it expires");
        }
    }
</script>

{#if session.state === "checking"}
    <main class="flex min-h-svh items-center justify-center bg-background p-4" aria-busy="true" aria-label="Opening the panel">
        <Skeleton class="h-48 w-full max-w-sm" />
    </main>
{:else if session.state === "unreachable"}
    <main class="flex min-h-svh flex-col items-center justify-center gap-4 bg-background p-4 text-center" role="alert">
        <h1 class="font-serif text-2xl font-semibold">The panel cannot reach its server</h1>
        <p class="max-w-md text-sm text-muted-foreground">
            The app that served this page did not answer. Check that it is still running, then try again.
        </p>
        <Button variant="outline" onclick={() => void probeSession()}><RotateCcwIcon />Try again</Button>
    </main>
{:else if session.state === "signed-out"}
    <SignIn />
{:else}
    <a
        href="#content"
        class="fixed top-2 left-2 z-50 -translate-y-20 rounded-md border bg-card px-3 py-2 text-sm shadow-xs focus:translate-y-0"
        onclick={(event) => {
            event.preventDefault();
            document.getElementById("content")?.focus();
        }}>Skip to content</a
    >
    <Sidebar.Provider>
        <Sidebar.Root collapsible="icon" variant="inset">
            <Sidebar.Header>
                <Sidebar.Menu>
                    <Sidebar.MenuItem>
                        <Sidebar.MenuButton size="lg">
                            {#snippet child({ props })}
                                <a href="#overview" {...props}>
                                    <div
                                        class="flex aspect-square size-8 items-center justify-center rounded-lg bg-primary font-serif text-lg font-bold text-primary-foreground"
                                    >
                                        A
                                    </div>
                                    <div class="grid flex-1 text-left leading-tight">
                                        <span class="truncate font-serif text-base font-semibold">Ambrose</span>
                                        <span class="truncate text-xs text-muted-foreground">{session.app || "Server panel"}</span>
                                    </div>
                                </a>
                            {/snippet}
                        </Sidebar.MenuButton>
                    </Sidebar.MenuItem>
                </Sidebar.Menu>
            </Sidebar.Header>
            <Sidebar.Content>
                {#each groups as group (group.heading)}
                    <Sidebar.Group>
                        <Sidebar.GroupLabel>{group.heading}</Sidebar.GroupLabel>
                        <Sidebar.GroupContent>
                            <Sidebar.Menu>
                                {#each group.entries as entry (entry.path)}
                                    <Sidebar.MenuItem>
                                        <Sidebar.MenuButton isActive={entry.path === path} tooltipContent={entry.title}>
                                            {#snippet child({ props })}
                                                <a href={`#${entry.path}`} {...props}>
                                                    <entry.icon />
                                                    <span>{entry.title}</span>
                                                </a>
                                            {/snippet}
                                        </Sidebar.MenuButton>
                                    </Sidebar.MenuItem>
                                {/each}
                            </Sidebar.Menu>
                        </Sidebar.GroupContent>
                    </Sidebar.Group>
                {/each}
            </Sidebar.Content>
            <Sidebar.Footer>
                <Sidebar.Menu>
                    <Sidebar.MenuItem>
                        <DropdownMenu.Root>
                            <DropdownMenu.Trigger>
                                {#snippet child({ props })}
                                    <Sidebar.MenuButton size="lg" {...props}>
                                        <div class="flex size-8 items-center justify-center rounded-lg bg-muted text-muted-foreground">
                                            <KeyRoundIcon class="size-4" />
                                        </div>
                                        <div class="grid flex-1 text-left text-sm leading-tight">
                                            <span class="truncate font-medium">Admin token</span>
                                            <span class="truncate text-xs text-muted-foreground"
                                                >{session.via === "session" ? "Browser session" : "Bearer token"}</span
                                            >
                                        </div>
                                        <ChevronsUpDownIcon class="ml-auto size-4" />
                                    </Sidebar.MenuButton>
                                {/snippet}
                            </DropdownMenu.Trigger>
                            <DropdownMenu.Content class="w-60 rounded-lg" side="right" align="end" sideOffset={4}>
                                <DropdownMenu.Label class="font-normal">
                                    <div class="text-sm font-medium">Signed in with the admin token</div>
                                    <div class="text-xs text-muted-foreground">
                                        Panel users with their own passwords arrive with milestone 17.46.
                                    </div>
                                </DropdownMenu.Label>
                                <DropdownMenu.Separator />
                                <DropdownMenu.Label class="text-xs font-normal text-muted-foreground">Theme</DropdownMenu.Label>
                                <DropdownMenu.RadioGroup
                                    value={theme.choice}
                                    onValueChange={(choice) => chooseTheme(choice as ThemeChoice)}
                                >
                                    <DropdownMenu.RadioItem value="system">Match the system</DropdownMenu.RadioItem>
                                    <DropdownMenu.RadioItem value="light">Light</DropdownMenu.RadioItem>
                                    <DropdownMenu.RadioItem value="dark">Dark</DropdownMenu.RadioItem>
                                </DropdownMenu.RadioGroup>
                                {#if session.via === "session"}
                                    <DropdownMenu.Separator />
                                    <DropdownMenu.Item onSelect={() => void leave()}><LogOutIcon />Sign out</DropdownMenu.Item>
                                {/if}
                            </DropdownMenu.Content>
                        </DropdownMenu.Root>
                    </Sidebar.MenuItem>
                </Sidebar.Menu>
            </Sidebar.Footer>
            <Sidebar.Rail />
        </Sidebar.Root>
        <Sidebar.Inset>
            <header
                class="sticky top-0 z-20 flex h-14 shrink-0 items-center gap-2 border-b bg-background/90 px-4 backdrop-blur md:rounded-t-xl"
            >
                <Sidebar.Trigger class="-ml-1" />
                <Separator orientation="vertical" class="mr-2 data-vertical:h-4 data-vertical:self-auto" />
                <Breadcrumb.Root>
                    <Breadcrumb.List>
                        <Breadcrumb.Item class="hidden md:block">
                            <Breadcrumb.Link href="#overview">Ambrose</Breadcrumb.Link>
                        </Breadcrumb.Item>
                        <Breadcrumb.Separator class="hidden md:block" />
                        <Breadcrumb.Item>
                            <Breadcrumb.Page>{title}</Breadcrumb.Page>
                        </Breadcrumb.Item>
                    </Breadcrumb.List>
                </Breadcrumb.Root>
                <Button
                    variant="outline"
                    size="sm"
                    class="ml-auto w-9 justify-start px-0 font-normal text-muted-foreground sm:w-64 sm:px-3"
                    aria-label="Search pages and servers"
                    onclick={() => (paletteOpen = true)}
                >
                    <SearchIcon class="mx-auto sm:mx-0" />
                    <span class="hidden truncate sm:inline">Search or jump to</span>
                    <Kbd.Group class="ml-auto hidden sm:inline-flex">
                        <Kbd.Root>{onMac ? "⌘" : "Ctrl"}</Kbd.Root>
                        <Kbd.Root>K</Kbd.Root>
                    </Kbd.Group>
                </Button>
                <StatusBadge tone={connection.tone} pulse={connection.tone === "healthy"} class="hidden md:inline-flex"
                    >{connection.word}</StatusBadge
                >
            </header>
            <main id="content" tabindex="-1" class="@container/main flex flex-1 flex-col gap-6 p-4 outline-none md:p-6">
                {#if live.connection === "reconnecting"}
                    <div
                        class="flex flex-wrap items-center gap-3 rounded-lg border border-waiting/40 bg-waiting/10 px-4 py-3 text-sm"
                        role="status"
                    >
                        <p class="flex-1">
                            The server stopped answering{live.error ? `: ${live.error.message}` : ""}. Attempt {live.attempt}, trying again
                            in {countdown} s.
                            {#if live.error?.requestId}<span class="font-mono text-xs select-all">Request {live.error.requestId}</span>{/if}
                        </p>
                        <Button size="sm" variant="outline" onclick={retryNow}><RotateCcwIcon />Retry now</Button>
                    </div>
                {:else if stale}
                    <div class="rounded-lg border px-4 py-3 text-sm text-muted-foreground" role="status">
                        The last sample is {formatAge(live.now - live.receivedAt)} old; the server is slow to answer.
                    </div>
                {/if}
                {#if shown.kind === "missing"}
                    <Denied title="No such page" detail={`Nothing in the panel lives at #${shown.path}.`} />
                {:else if shown.kind === "refused"}
                    <Denied
                        title="Access denied"
                        detail={`${shown.route.title} needs the ${shown.route.permission} permission, which this sign-in does not grant.`}
                    />
                {:else if shown.route.view.kind === "denied"}
                    <Denied title="Access denied" detail="This page is where a link you may not follow lands." />
                {:else if shown.route.view.kind === "arrives"}
                    {#key shown.route.path}
                        <Unavailable
                            path={shown.route.path}
                            title={shown.route.title}
                            milestone={shown.route.view.milestone}
                            preview={shown.route.view.preview}
                        />
                    {/key}
                {:else}
                    {#await shown.route.view.load()}
                        <div class="space-y-3" aria-busy="true" aria-label={`Loading ${shown.route.title}`}>
                            <Skeleton class="h-9 w-56" />
                            <Skeleton class="h-4 w-80 max-w-full" />
                            <Skeleton class="h-48 w-full" />
                        </div>
                    {:then loaded}
                        <loaded.default />
                    {:catch}
                        <div
                            role="alert"
                            class="flex flex-1 flex-col items-center justify-center gap-4 rounded-xl border border-destructive/30 bg-destructive/5 p-12 text-center"
                        >
                            <h1 class="font-serif text-2xl font-semibold">{shown.route.title} did not load</h1>
                            <p class="max-w-md text-sm text-muted-foreground">
                                The panel could not fetch this page's code, usually because a newer build replaced the one this tab started
                                with, or the connection dropped.
                            </p>
                            <Button variant="outline" onclick={() => window.location.reload()}><RotateCcwIcon />Reload the panel</Button>
                        </div>
                    {/await}
                {/if}
            </main>
        </Sidebar.Inset>
    </Sidebar.Provider>
    <CommandPalette bind:open={paletteOpen} pages={reachable} />
{/if}
<Toaster position="bottom-right" />
