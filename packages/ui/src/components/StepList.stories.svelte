<!-- Project Ambrose by Imjustchico: What stands in for a spinner everywhere in Ambrose: steps with their own states and their own real figures. -->
<script module lang="ts">
    import { defineMeta } from "@storybook/addon-svelte-csf";
    import StepList from "./StepList.svelte";

    const { Story } = defineMeta({
        title: "State/StepList",
        component: StepList,
        tags: ["autodocs"],
    });

    const firstRun = [
        { id: "install", label: "Find the installation", state: "healthy" as const, word: "Found", detail: "V_r806919" },
        { id: "dump", label: "Build the type dump", state: "healthy" as const, word: "Done", detail: "6,982 classes" },
        { id: "names", label: "Read the name tables", state: "waiting" as const, word: "Reading", detail: "18 of 40" },
        { id: "world", label: "Import the world data", state: "unknown" as const, word: "Waiting" },
    ];

    const failed = [
        { id: "install", label: "Find the installation", state: "healthy" as const, word: "Found", detail: "V_r806919" },
        { id: "dump", label: "Build the type dump", state: "wrong" as const, word: "Refused", detail: "no dump for this revision" },
    ];
</script>

<Story name="First run">
    {#snippet template()}
        <StepList label="First run" steps={firstRun} />
    {/snippet}
</Story>

<Story name="One step went wrong">
    {#snippet template()}
        <StepList label="First run" steps={failed} />
    {/snippet}
</Story>
