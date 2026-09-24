<!-- Project Ambrose by Imjustchico: Numbers, paths and revisions lining up down a column. -->
<script module lang="ts">
    import { defineMeta } from "@storybook/addon-svelte-csf";
    import Mono from "./Mono.svelte";

    const { Story } = defineMeta({
        title: "Foundations/Mono",
        component: Mono,
        tags: ["autodocs"],
    });
</script>

<Story name="Numbers line up">
    {#snippet template()}
        <div class="flex flex-col gap-4">
            <Mono>1,284</Mono>
            <Mono>9,113</Mono>
            <Mono>112</Mono>
        </div>
    {/snippet}
</Story>

<Story name="Tones and sizes">
    {#snippet template()}
        <div class="flex flex-col gap-6">
            <Mono size="26">42 ms</Mono>
            <Mono tone="muted">V_r806919.Wizard_1_610</Mono>
            <Mono tone="faint" size="11">2026-09-18 12:04:11</Mono>
        </div>
    {/snippet}
</Story>
