<!-- Project Ambrose by Imjustchico: A live setting's history in a side sheet, read from the app when the sheet opens: each change newest first with when, who, from where and why, and the values before and after, a secret's only as its mask, with a revert beside each row that opens the change dialog at the value the row replaced; a secret's row offers none, because its history keeps no value to return to. -->
<script lang="ts">
    import * as Sheet from "$lib/components/ui/sheet/index.js";
    import { Button } from "$lib/components/ui/button/index.js";
    import { ApiError } from "$lib/api.svelte.js";
    import { settingHistory } from "$lib/supervision.svelte.js";
    import type { SettingHistoryAnswer } from "$lib/schemas.js";
    import type { Setting } from "$lib/settings.js";
    import Undo2Icon from "@lucide/svelte/icons/undo-2";

    type Entry = SettingHistoryAnswer["entries"][number];
    type Props = {
        open: boolean;
        app: string;
        setting: Setting | null;
        canChange: boolean;
        revert: (entry: Entry) => void;
    };

    let { open = $bindable(), app, setting, canChange, revert }: Props = $props();

    let answer = $state<SettingHistoryAnswer | null>(null);
    let failure = $state("");

    $effect(() => {
        const key = setting?.key;
        if (!open || !key) return;
        const controller = new AbortController();
        answer = null;
        failure = "";
        void (async () => {
            try {
                answer = await settingHistory(app, key, controller.signal);
            } catch (problem) {
                if (controller.signal.aborted) return;
                failure = problem instanceof ApiError ? problem.message : `The history of ${key} could not be read`;
            }
        })();
        return () => controller.abort();
    });

    function when(seconds: number): string {
        return new Date(seconds * 1000).toLocaleString();
    }
</script>

<Sheet.Root bind:open>
    <Sheet.Content side="right" class="w-full overflow-y-auto sm:max-w-lg">
        <Sheet.Header>
            <Sheet.Title>History of {setting?.key ?? ""}</Sheet.Title>
            <Sheet.Description>Every change made while the app ran, newest first, with who made it and why.</Sheet.Description>
        </Sheet.Header>
        <div class="space-y-3 px-4 pb-6">
            {#if failure}
                <p class="text-sm text-destructive" role="alert">{failure}</p>
            {:else if !answer}
                <p class="text-sm text-muted-foreground">Reading the history.</p>
            {:else if answer.entries.length === 0}
                <p class="text-sm text-muted-foreground">It has never been changed while the app ran.</p>
            {:else}
                <ol class="space-y-3">
                    {#each answer.entries as entry (entry.id)}
                        <li class="rounded-md border p-3 text-sm">
                            <div class="flex flex-wrap items-baseline justify-between gap-2">
                                <span class="font-medium">{entry.who === "" ? "Someone unnamed" : entry.who}</span>
                                <span class="text-xs text-muted-foreground">{when(entry.epoch_seconds)} · {entry.source}</span>
                            </div>
                            <div class="mt-1 font-mono text-xs break-all">
                                {entry.old === "" ? "empty" : entry.old} → {entry.new === "" ? "empty" : entry.new}
                            </div>
                            {#if entry.reason}<p class="mt-1 text-muted-foreground">{entry.reason}</p>{/if}
                            {#if canChange && answer.visibility !== "secret"}
                                <Button
                                    variant="outline"
                                    size="sm"
                                    class="mt-2"
                                    aria-label={`Revert to ${entry.old === "" ? "empty" : entry.old}`}
                                    onclick={() => revert(entry)}
                                >
                                    <Undo2Icon />Revert to this value
                                </Button>
                            {/if}
                        </li>
                    {/each}
                </ol>
                {#if answer.visibility === "secret"}
                    <p class="text-xs text-muted-foreground">A secret's history holds only its mask, so there is no value to revert to.</p>
                {/if}
            {/if}
        </div>
    </Sheet.Content>
</Sheet.Root>
