<!-- Project Ambrose by Imjustchico: The three accessibility criteria no automated gate can judge, each as a test that fails when the rule is broken. -->
<script module lang="ts">
    import { defineMeta } from "@storybook/addon-svelte-csf";
    import { expect } from "storybook/test";
    import Button from "../components/Button.svelte";
    import TextField from "../components/TextField.svelte";

    const { Story } = defineMeta({
        title: "Gallery/Criteria",
    });

    const rows = Array.from({ length: 24 }, (_value, index) => index + 1);

    function floorForPointer(): number {
        return window.matchMedia("(pointer: coarse)").matches ? 44 : 24;
    }
</script>

<Story
    name="A focused row is not hidden under the sticky chrome"
    play={async ({ canvas }) => {
        const scroller = canvas.getByTestId("criteria-scroller");
        const chrome = canvas.getByTestId("criteria-chrome");
        const frame = () => new Promise((settle) => requestAnimationFrame(() => requestAnimationFrame(settle)));
        await expect(scroller.scrollHeight).toBeGreaterThan(scroller.clientHeight);
        scroller.scrollTop = scroller.scrollHeight;
        await frame();
        await expect(scroller.scrollTop).toBeGreaterThan(0);
        const row = canvas.getByRole("button", { name: "Row 3" });
        row.focus();
        row.scrollIntoView({ block: "start" });
        await frame();
        const focused = row.getBoundingClientRect();
        const bar = chrome.getBoundingClientRect();
        await expect(bar.height).toBeGreaterThan(0);
        await expect(focused.top).toBeGreaterThanOrEqual(bar.bottom - 0.5);
    }}
>
    {#snippet template()}
        <div
            data-testid="criteria-scroller"
            data-ambrose-scroll
            style="--ambrose-chrome: 40px; height: 160px"
            class="relative overflow-y-auto rounded-card border border-edge-quiet bg-surface-card"
        >
            <div
                data-testid="criteria-chrome"
                style="height: 40px"
                class="sticky top-0 z-10 flex items-center bg-surface-chrome px-12 text-13 text-fg-body"
            >
                Sessions
            </div>
            <ul class="flex list-none flex-col gap-2 p-0">
                {#each rows as row (row)}
                    <li><Button variant="ghost">Row {row}</Button></li>
                {/each}
            </ul>
        </div>
    {/snippet}
</Story>

<Story
    name="Every target clears the floor for the pointer in use"
    play={async ({ canvas }) => {
        const floor = floorForPointer();
        const targets = canvas.getAllByRole("button");
        await expect(targets.length).toBeGreaterThan(0);
        for (const target of targets) {
            const box = target.getBoundingClientRect();
            await expect(Math.round(Math.min(box.width, box.height))).toBeGreaterThanOrEqual(floor);
        }
    }}
>
    {#snippet template()}
        <div class="flex items-center gap-12">
            <Button variant="action">Start server</Button>
            <Button variant="quiet">Cancel</Button>
            <Button variant="ghost">Details</Button>
        </div>
    {/snippet}
</Story>

<Story
    name="An authentication field takes a pasted value"
    play={async ({ canvas }) => {
        const field = canvas.getByLabelText("Password") as HTMLInputElement;
        await expect(field.type).toBe("password");
        await expect(field.autocomplete).toBe("current-password");
        const clipboard = new DataTransfer();
        clipboard.setData("text/plain", "a-long-passphrase-from-a-manager");
        const paste = new ClipboardEvent("paste", { clipboardData: clipboard, bubbles: true, cancelable: true });
        field.focus();
        const delivered = field.dispatchEvent(paste);
        await expect(delivered).toBe(true);
        field.value = clipboard.getData("text/plain");
        field.dispatchEvent(new Event("input", { bubbles: true }));
        await expect(field.value).toBe("a-long-passphrase-from-a-manager");
    }}
>
    {#snippet template()}
        <TextField id="criteria-password" label="Password" type="password" autocomplete="current-password" />
    {/snippet}
</Story>
