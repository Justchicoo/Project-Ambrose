<!-- Project Ambrose by Imjustchico: The one dialog every change to a live setting goes through: an edit or a revert to a value from its history has a typed and bounded input, masked for a secret, that keeps a value the server would refuse from being sent, a reason that is required, and a review step showing from what to what and when it takes hold before anything is applied, while a reset to the config value is confirmed in one step with its reason and when it takes hold; a refusal from the server is shown with its own message and the step goes back to the edit. -->
<script lang="ts">
    import * as Dialog from "$lib/components/ui/dialog/index.js";
    import { Button } from "$lib/components/ui/button/index.js";
    import { Input } from "$lib/components/ui/input/index.js";
    import { Label } from "$lib/components/ui/label/index.js";
    import { Switch } from "$lib/components/ui/switch/index.js";
    import { Textarea } from "$lib/components/ui/textarea/index.js";
    import { ApiError } from "$lib/api.svelte.js";
    import { changeSetting, resetSetting } from "$lib/supervision.svelte.js";
    import { applyPhrase, checkValue, layerName, normalise, textBytes, type Setting } from "$lib/settings.js";

    type Props = {
        open: boolean;
        app: string;
        setting: Setting | null;
        mode: "edit" | "reset";
        value: string;
        reason: string;
        done: (message: string) => void;
    };

    let { open = $bindable(), app, setting, mode, value = $bindable(), reason = $bindable(), done }: Props = $props();

    const MaxReasonBytes = 255;
    let step = $state<"edit" | "review">("edit");
    let busy = $state(false);
    let failure = $state("");

    $effect(() => {
        if (open) {
            step = "edit";
            failure = "";
        }
    });

    const signed = $derived((setting?.min ?? "").trim().startsWith("-"));
    const keypad = $derived(
        setting?.type === "unsigned" || (setting?.type === "integer" && !signed)
            ? "numeric"
            : setting?.type === "float" && !signed
              ? "decimal"
              : undefined,
    );
    const long = $derived(setting?.type === "string" && Number(setting.max ?? "65535") > 255);
    const problem = $derived(setting && mode === "edit" ? checkValue(setting, value) : null);
    const reasonProblem = $derived(
        reason.trim() === ""
            ? "Say why, so the history can tell whoever reads it later"
            : textBytes(reason.trim()) > MaxReasonBytes
              ? `A reason is at most ${MaxReasonBytes} bytes`
              : null,
    );
    const next = $derived(setting && mode === "edit" ? normalise(setting, value) : "");
    const same = $derived(setting !== null && mode === "edit" && next === setting.value && !setting.secret);
    const ready = $derived(setting !== null && problem === null && reasonProblem === null && !same);

    async function apply() {
        if (!setting || !ready || busy) return;
        busy = true;
        failure = "";
        try {
            const answer =
                mode === "reset"
                    ? await resetSetting(app, setting.key, reason.trim())
                    : await changeSetting(app, setting.key, next, reason.trim());
            open = false;
            done(answer.message);
        } catch (refusal) {
            if (refusal instanceof ApiError)
                failure = refusal.fields.value ? `${refusal.message}: ${refusal.fields.value}` : refusal.message;
            else failure = `${setting.key} could not be changed`;
            step = "edit";
        } finally {
            busy = false;
        }
    }
</script>

<Dialog.Root bind:open>
    <Dialog.Content class="sm:max-w-lg">
        {#if setting}
            <Dialog.Header>
                <Dialog.Title>
                    {#if mode === "reset"}Return {setting.key} to its config value?{:else if step === "review"}Apply this change to {setting.key}?{:else}Change
                        {setting.key}{/if}
                </Dialog.Title>
                <Dialog.Description>{setting.description ?? ""}</Dialog.Description>
            </Dialog.Header>

            {#if failure}
                <p class="rounded-md border border-destructive/30 bg-destructive/5 p-3 text-sm text-destructive" role="alert">{failure}</p>
            {/if}

            {#if step === "edit"}
                <div class="space-y-4">
                    {#if mode === "edit"}
                        <div class="space-y-2">
                            <Label for="setting-value">New value{setting.unit ? ` (${setting.unit})` : ""}</Label>
                            {#if setting.type === "bool"}
                                <div class="flex items-center gap-3">
                                    <Switch
                                        id="setting-value"
                                        checked={normalise(setting, value) === "true"}
                                        onCheckedChange={(checked) => (value = checked ? "true" : "false")}
                                    />
                                    <span class="text-sm">{normalise(setting, value) === "true" ? "On" : "Off"}</span>
                                </div>
                            {:else if setting.secret}
                                <Input
                                    id="setting-value"
                                    type="password"
                                    autocomplete="off"
                                    autocapitalize="off"
                                    spellcheck={false}
                                    bind:value
                                    aria-invalid={problem !== null}
                                />
                            {:else if long}
                                <Textarea id="setting-value" bind:value rows={3} aria-invalid={problem !== null} />
                            {:else}
                                <Input
                                    id="setting-value"
                                    type="text"
                                    inputmode={keypad}
                                    autocomplete="off"
                                    bind:value
                                    aria-invalid={problem !== null}
                                />
                            {/if}
                            {#if problem}
                                <p class="text-xs text-destructive" role="alert">{problem}</p>
                            {:else if same}
                                <p class="text-xs text-muted-foreground">That is the value it already holds.</p>
                            {:else}
                                <p class="text-xs text-muted-foreground">
                                    {setting.bounds ? `${setting.bounds[0].toUpperCase()}${setting.bounds.slice(1)}. ` : ""}Default {setting.declared_default ===
                                    ""
                                        ? "empty"
                                        : setting.declared_default}{setting.unit ? ` ${setting.unit}` : ""}.
                                </p>
                            {/if}
                        </div>
                    {:else}
                        <p class="text-sm">
                            {setting.key} goes back to the value its config gives it, and the value set live is removed. It holds
                            <span class="font-mono break-all">{setting.value === "" ? "nothing" : setting.value}</span> now, from {layerName(
                                setting.layer,
                            ).toLowerCase()}.
                        </p>
                        <p class="text-sm text-muted-foreground">{applyPhrase(setting)}.</p>
                    {/if}
                    <div class="space-y-2">
                        <Label for="setting-reason">Why</Label>
                        <Textarea
                            id="setting-reason"
                            bind:value={reason}
                            rows={2}
                            placeholder="What this change is for"
                            aria-invalid={reasonProblem !== null && reason !== ""}
                        />
                        {#if reasonProblem && reason !== ""}<p class="text-xs text-destructive">{reasonProblem}</p>{/if}
                    </div>
                </div>
                <Dialog.Footer>
                    <Button variant="outline" onclick={() => (open = false)}>Leave it</Button>
                    {#if mode === "reset"}
                        <Button disabled={reasonProblem !== null || busy} onclick={() => void apply()}
                            >{busy ? "Resetting…" : "Reset it"}</Button
                        >
                    {:else}
                        <Button disabled={!ready} onclick={() => (step = "review")}>Review</Button>
                    {/if}
                </Dialog.Footer>
            {:else}
                <dl class="grid grid-cols-[auto_1fr] gap-x-4 gap-y-2 text-sm">
                    <dt class="text-muted-foreground">From</dt>
                    <dd class="font-mono break-all">{setting.value === "" ? "empty" : setting.value}</dd>
                    <dt class="text-muted-foreground">To</dt>
                    <dd class="font-mono break-all">{setting.secret ? "the new secret you typed" : next === "" ? "empty" : next}</dd>
                    <dt class="text-muted-foreground">When</dt>
                    <dd>{applyPhrase(setting)}</dd>
                    <dt class="text-muted-foreground">Why</dt>
                    <dd class="break-words">{reason.trim()}</dd>
                </dl>
                <Dialog.Footer>
                    <Button variant="outline" onclick={() => (step = "edit")}>Back</Button>
                    <Button disabled={busy} onclick={() => void apply()}>{busy ? "Applying…" : "Apply"}</Button>
                </Dialog.Footer>
            {/if}
        {/if}
    </Dialog.Content>
</Dialog.Root>
