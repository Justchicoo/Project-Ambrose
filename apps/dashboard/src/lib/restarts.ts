/*
 * Project Ambrose by Imjustchico
 * The changes that need a server restarted rather than reloaded, each with why, as doc/OPERATIONS.md lists them under Restart-required changes; the reload page shows them beside the stores it can rebuild, and a test holds this list to that table word for word.
 */

export type RestartCase = { change: string; reason: string };

export const restartCases: RestartCase[] = [
    { change: "Binary upgrade", reason: "The running process cannot replace its executable and code safely." },
    { change: "Adding or removing a compiled module", reason: "Module code and registration are fixed when the process starts." },
    { change: "A schema update the new binary needs", reason: "The old binary may not understand the new schema or statements." },
    {
        change: "A client revision or type dump change when live objects cannot hold the old registry",
        reason: "Existing objects retain the old registry and cannot safely cross the incompatible boundary.",
    },
    { change: "Client-side WAD changes", reason: "The client must be re-patched before it can use the changed archive." },
];
