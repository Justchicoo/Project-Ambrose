-- Project Ambrose by Imjustchico
-- Holds the panel's port allocations and per-node port pools: one row per socket an app listens on, with its bind address, port, protocol, public alias, the app and role using it and notes, unique by node, address, port and protocol, seeded from the shipped defaults, and one port range per node that supplies ports for new realms.
CREATE TABLE port_allocation (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    node TEXT NOT NULL DEFAULT 'local',
    bind_address TEXT NOT NULL,
    port INTEGER NOT NULL CHECK (port >= 1 AND port <= 65535),
    protocol TEXT NOT NULL DEFAULT 'tcp' CHECK (protocol IN ('tcp', 'udp', 'http')),
    public_alias TEXT NOT NULL DEFAULT '',
    app TEXT NOT NULL DEFAULT '',
    role TEXT NOT NULL DEFAULT '',
    notes TEXT NOT NULL DEFAULT '',
    created_epoch_ms INTEGER NOT NULL,
    UNIQUE (node, bind_address, port, protocol)
);

CREATE INDEX idx_port_allocation_app ON port_allocation (app, role);

CREATE TABLE port_pool (
    node TEXT PRIMARY KEY,
    start_port INTEGER NOT NULL CHECK (start_port >= 1 AND start_port <= 65535),
    end_port INTEGER NOT NULL CHECK (end_port >= 1 AND end_port <= 65535),
    CHECK (start_port <= end_port)
);

INSERT INTO port_allocation (node, bind_address, port, protocol, public_alias, app, role, notes, created_epoch_ms) VALUES
    ('local', '0.0.0.0', 12000, 'tcp', '', 'loginserver', 'login listener', 'Shipped default LoginServerPort', 0),
    ('local', '0.0.0.0', 12333, 'tcp', '', 'gameserver', 'game listener', 'Shipped default WorldServerPort', 0),
    ('local', '0.0.0.0', 12500, 'tcp', '', 'patchserver', 'patch HTTP', 'Shipped default PatchServerPort', 0),
    ('local', '127.0.0.1', 12010, 'tcp', '', 'loginserver', 'admin API', 'Shipped default Admin.Port; also serves /metrics', 0),
    ('local', '127.0.0.1', 12343, 'tcp', '', 'gameserver', 'admin API', 'Shipped default Admin.Port; also serves /metrics', 0),
    ('local', '127.0.0.1', 12510, 'tcp', '', 'patchserver', 'admin API', 'Shipped default Admin.Port; also serves /metrics', 0),
    ('local', '127.0.0.1', 12020, 'tcp', '', 'supervisor', 'admin API', 'Shipped default Admin.Port; also serves /metrics', 0),
    ('local', '127.0.0.1', 12080, 'tcp', '', 'supervisor', 'panel', 'Shipped default Panel.Port', 0);

INSERT INTO port_pool (node, start_port, end_port) VALUES ('local', 13000, 13999);
