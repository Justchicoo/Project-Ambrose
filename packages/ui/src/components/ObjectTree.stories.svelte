<!-- Project Ambrose by Imjustchico: The typed property tree with nested objects, flags, enum options and paged containers. -->
<script module lang="ts">
    import { defineMeta } from "@storybook/addon-svelte-csf";
    import ObjectTree, { type TreeNode } from "./ObjectTree.svelte";

    const { Story } = defineMeta({
        title: "Data/ObjectTree",
        component: ObjectTree,
        tags: ["autodocs"],
    });

    const properties: TreeNode[] = [
        {
            name: "m_name",
            type: "string",
            detail: "id 1 · offset 8 · hash 123456",
            flags: ["Serialized"],
        },
        {
            name: "m_school",
            type: "School",
            detail: "id 2 · offset 16 · hash 234567",
            flags: ["Serialized"],
            enumOptions: [
                { name: "Fire", value: 0 },
                { name: "Ice", value: 1 },
                { name: "Storm", value: 2 },
            ],
        },
        {
            name: "m_stats",
            type: "PlayerStats",
            detail: "id 3 · offset 24 · hash 345678",
            children: [
                { name: "m_health", type: "int", detail: "id 1 · offset 0 · hash 111" },
                { name: "m_mana", type: "int", detail: "id 2 · offset 4 · hash 222" },
            ],
        },
    ];

    const longList: TreeNode[] = [
        {
            name: "m_items",
            type: "Item[]",
            detail: "id 4 · offset 32 · hash 456789",
            totalChildren: 60,
            children: Array.from({ length: 25 }, (_, i) => ({
                name: `m_items[${i}]`,
                type: "Item",
                detail: `id ${i} · offset ${i * 8}`,
            })),
            loadMore: async () => [],
        },
    ];
</script>

<Story name="Properties">
    {#snippet template()}
        <ObjectTree nodes={properties} />
    {/snippet}
</Story>

<Story name="PagedContainer">
    {#snippet template()}
        <ObjectTree nodes={longList} />
    {/snippet}
</Story>
