<!-- Project Ambrose by Imjustchico: A hint that appears beside a control on hover and on focus, never the only place a meaning is written. -->
<script lang="ts">
    import type { Snippet } from "svelte";
    import { Tooltip } from "bits-ui";

    type Props = {
        text: string;
        delay?: number;
        children: Snippet<[Record<string, unknown>]>;
    };

    let { text, delay = 200, children }: Props = $props();
</script>

<Tooltip.Provider>
    <Tooltip.Root delayDuration={delay}>
        <Tooltip.Trigger>
            {#snippet child({ props })}
                {@render children(props)}
            {/snippet}
        </Tooltip.Trigger>
        <Tooltip.Portal>
            <Tooltip.Content
                sideOffset={6}
                class="ambrose-flip z-50 rounded-control border border-edge-strong bg-surface-chrome px-10 py-6 text-12 text-fg-body"
            >
                {text}
            </Tooltip.Content>
        </Tooltip.Portal>
    </Tooltip.Root>
</Tooltip.Provider>
