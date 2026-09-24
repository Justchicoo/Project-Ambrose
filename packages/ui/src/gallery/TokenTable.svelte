<!-- Project Ambrose by Imjustchico: The gallery's tokens page: every meaning with its value in both themes, and the contrast each documented pair reaches against the ratio it has to clear. -->
<script lang="ts">
    import { contrastPairs, semanticColors, seriesColors, space, fontSize, durationMs, type Theme } from "../tokens/tokens";
    import Heading from "../components/Heading.svelte";

    type Props = {
        theme?: Theme;
    };

    let { theme = "dark" }: Props = $props();

    const names = Object.keys(semanticColors.dark) as (keyof typeof semanticColors.dark)[];
    const pairs = $derived(contrastPairs.filter((pair) => pair.theme === theme));
</script>

<div class="flex flex-col gap-28">
    <section class="flex flex-col gap-12">
        <Heading level={2}>Meanings</Heading>
        <table class="w-full border-collapse text-13">
            <caption class="ambrose-label py-6 text-start">Every semantic token in both themes</caption>
            <thead>
                <tr class="border-b border-edge-quiet">
                    <th scope="col" class="py-8 text-start">Token</th>
                    <th scope="col" class="py-8 text-start">Dark</th>
                    <th scope="col" class="py-8 text-start">Light</th>
                </tr>
            </thead>
            <tbody>
                {#each names as name (name)}
                    <tr class="border-b border-edge-quiet">
                        <th scope="row" class="py-8 text-start font-normal text-fg-body">{name}</th>
                        <td class="py-8"><span class="ambrose-mono text-12 text-fg-muted">{semanticColors.dark[name]}</span></td>
                        <td class="py-8"><span class="ambrose-mono text-12 text-fg-muted">{semanticColors.light[name]}</span></td>
                    </tr>
                {/each}
            </tbody>
        </table>
    </section>

    <section class="flex flex-col gap-12">
        <Heading level={2}>Contrast</Heading>
        <table class="w-full border-collapse text-13">
            <caption class="ambrose-label py-6 text-start">What every documented pair reaches on the {theme} theme</caption>
            <thead>
                <tr class="border-b border-edge-quiet">
                    <th scope="col" class="py-8 text-start">Text</th>
                    <th scope="col" class="py-8 text-start">Ground</th>
                    <th scope="col" class="py-8 text-end">Reaches</th>
                    <th scope="col" class="py-8 text-end">Needs</th>
                </tr>
            </thead>
            <tbody>
                {#each pairs as pair (pair.foreground + pair.background + pair.theme)}
                    <tr class="border-b border-edge-quiet">
                        <th scope="row" class="py-6 text-start font-normal text-fg-body">{pair.foreground}</th>
                        <td class="py-6 text-fg-muted">{pair.background}</td>
                        <td class="ambrose-mono py-6 text-end text-state-healthy">{pair.ratio.toFixed(2)}</td>
                        <td class="ambrose-mono py-6 text-end text-fg-faint">{pair.minimum.toFixed(1)}</td>
                    </tr>
                {/each}
            </tbody>
        </table>
    </section>

    <section class="flex flex-col gap-12">
        <Heading level={2}>Series</Heading>
        <ol class="flex list-none flex-wrap gap-8 p-0">
            {#each seriesColors[theme] as value, index (value)}
                <li class="flex items-center gap-8 rounded-input border border-edge-quiet px-10 py-6">
                    <span class="ambrose-mono text-12 text-fg-muted">series-{index + 1}</span>
                    <span class="ambrose-mono text-12 text-fg-faint">{value}</span>
                </li>
            {/each}
        </ol>
    </section>

    <section class="flex flex-col gap-12">
        <Heading level={2}>Scales</Heading>
        <p class="text-13 text-fg-muted">Spacing {Object.values(space).join(", ")}</p>
        <p class="text-13 text-fg-muted">Type {Object.values(fontSize).join(", ")}</p>
        <p class="text-13 text-fg-muted">
            Motion {Object.entries(durationMs)
                .map(([name, value]) => `${name} ${value} ms`)
                .join(", ")}
        </p>
    </section>
</div>
