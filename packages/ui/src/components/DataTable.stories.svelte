<!-- Project Ambrose by Imjustchico: A real table with rows, sorted, and with nothing in it. -->
<script module lang="ts">
    import { defineMeta } from "@storybook/addon-svelte-csf";
    import DataTable from "./DataTable.svelte";

    const { Story } = defineMeta({
        title: "Data/DataTable",
        component: DataTable,
        tags: ["autodocs"],
    });

    type Server = { name: string; state: string; players: number; uptime: string };

    const rows: Server[] = [
        { name: "Ravenwood", state: "Running", players: 128, uptime: "4d 02:11" },
        { name: "Krokotopia", state: "Stopped", players: 0, uptime: "-" },
        { name: "Marleybone", state: "Running", players: 41, uptime: "0d 06:40" },
    ];

    const columns = [
        { id: "name", header: "Server", value: (row: Server) => row.name },
        { id: "state", header: "State", value: (row: Server) => row.state },
        { id: "players", header: "Players", value: (row: Server) => String(row.players), mono: true, align: "end" as const },
        { id: "uptime", header: "Uptime", value: (row: Server) => row.uptime, mono: true, align: "end" as const },
    ];
</script>

<Story name="With rows">
    {#snippet template()}
        <DataTable caption="Servers on this machine" {columns} {rows} />
    {/snippet}
</Story>

<Story name="Empty">
    {#snippet template()}
        <DataTable caption="Servers on this machine" {columns} rows={[]} empty="No server is registered on this machine yet." />
    {/snippet}
</Story>
