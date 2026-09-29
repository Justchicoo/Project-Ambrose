<!-- Project Ambrose by Imjustchico: The one time an operator sees their recovery codes: a dialog with no close button that neither a click outside nor Escape dismisses, the ten codes grouped for reading, a copy and a download as a text file, and a confirmation that stays off until they say the codes are saved, after which the codes are handed back to be dropped and nothing of them is kept in the page or the browser. -->
<script lang="ts">
    import * as Dialog from "$lib/components/ui/dialog/index.js";
    import { Button } from "$lib/components/ui/button/index.js";
    import { Label } from "$lib/components/ui/label/index.js";
    import { Switch } from "$lib/components/ui/switch/index.js";
    import { groupRecoveryCode, recoveryCodesText } from "$lib/twofactor.js";
    import CopyIcon from "@lucide/svelte/icons/copy";
    import DownloadIcon from "@lucide/svelte/icons/download";

    type Props = {
        open: boolean;
        codes: string[];
        account: string;
        issuer: string;
        saved: () => void;
    };

    let { open = $bindable(), codes, account, issuer, saved }: Props = $props();

    let stored = $state(false);
    let notice = $state("");

    $effect(() => {
        if (open) {
            stored = false;
            notice = "";
        }
    });

    const text = $derived(recoveryCodesText(codes, account, issuer, new Date()));

    async function copy() {
        try {
            await navigator.clipboard.writeText(text);
            notice = "Copied. Paste them somewhere only you can reach.";
        } catch {
            notice = "The browser did not allow copying; download them instead.";
        }
    }

    function download() {
        const link = document.createElement("a");
        const address = URL.createObjectURL(new Blob([text], { type: "text/plain;charset=utf-8" }));
        link.href = address;
        link.download = "ambrose-recovery-codes.txt";
        link.click();
        URL.revokeObjectURL(address);
        notice = "Downloaded as ambrose-recovery-codes.txt.";
    }

    function finish() {
        if (!stored) return;
        open = false;
        notice = "";
        stored = false;
        saved();
    }
</script>

<Dialog.Root bind:open>
    <Dialog.Content class="sm:max-w-md" showCloseButton={false} interactOutsideBehavior="ignore" escapeKeydownBehavior="ignore">
        <Dialog.Header>
            <Dialog.Title>Save your recovery codes</Dialog.Title>
            <Dialog.Description>
                Each code signs you in once when your authenticator app is not to hand. This is the only time the panel shows them; it keeps
                nothing it could show again.
            </Dialog.Description>
        </Dialog.Header>
        <ol class="grid grid-cols-2 gap-2 rounded-lg border bg-muted/40 p-3 font-mono text-sm" aria-label="Recovery codes">
            {#each codes as code (code)}
                <li class="select-all">{groupRecoveryCode(code)}</li>
            {/each}
        </ol>
        <div class="flex flex-wrap gap-2">
            <Button variant="outline" onclick={() => void copy()}><CopyIcon />Copy</Button>
            <Button variant="outline" onclick={download}><DownloadIcon />Download as text</Button>
        </div>
        {#if notice}
            <p class="text-xs text-muted-foreground" role="status">{notice}</p>
        {/if}
        <div class="flex items-center gap-3">
            <Switch id="recovery-codes-stored" bind:checked={stored} />
            <Label for="recovery-codes-stored">I have saved these codes somewhere safe</Label>
        </div>
        <Dialog.Footer>
            {#if !stored}
                <p class="mr-auto self-center text-xs text-muted-foreground">Say you have saved them to carry on.</p>
            {/if}
            <Button onclick={finish} disabled={!stored}>Done</Button>
        </Dialog.Footer>
    </Dialog.Content>
</Dialog.Root>
