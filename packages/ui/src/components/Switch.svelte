<!-- Project Ambrose by Imjustchico: A setting that is on or off right now, which is why it says on or off in words beside the control. -->
<script lang="ts">
    import { Switch } from "bits-ui";
    import { classes } from "../internal/classes";

    type Props = {
        id: string;
        label: string;
        checked?: boolean;
        disabled?: boolean;
        onWord?: string;
        offWord?: string;
        class?: string;
    };

    let { id, label, checked = $bindable(false), disabled = false, onWord = "On", offWord = "Off", class: extra }: Props = $props();
</script>

<div class={classes("flex min-h-44 items-center justify-between gap-16", extra)}>
    <label for={id} class="text-15 text-fg-body">{label}</label>
    <div class="flex items-center gap-10">
        <span class="text-12 text-fg-muted">{checked ? onWord : offWord}</span>
        <Switch.Root
            {id}
            bind:checked
            {disabled}
            class="ambrose-flip inline-flex h-22 w-40 shrink-0 items-center rounded-pill border border-edge-strong bg-surface-sunken px-4 data-[state=checked]:bg-fill-action disabled:opacity-50"
        >
            <Switch.Thumb
                class="ambrose-flip block h-14 w-14 rounded-pill bg-fg-muted data-[state=checked]:translate-x-16 data-[state=checked]:bg-on-fill"
            />
        </Switch.Root>
    </div>
</div>
