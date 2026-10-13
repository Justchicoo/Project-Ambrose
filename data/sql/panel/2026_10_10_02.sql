-- Project Ambrose by Imjustchico
-- Holds each app's launch settings 17.28's operators edit from the panel: one row per setting with the app it belongs to, a secret flag saying the value is sealed with AES-256-GCM under the supervisor's key rather than plain text, the keyring key that sealed it, and who changed it last, so the supervisor launches an app from these without reading a service file.
CREATE TABLE launch_settings (
    app TEXT NOT NULL,
    name TEXT NOT NULL,
    value TEXT NOT NULL DEFAULT '',
    secret INTEGER NOT NULL DEFAULT 0,
    key_id INTEGER NOT NULL DEFAULT 0,
    updated_by TEXT NOT NULL DEFAULT '',
    updated_epoch_ms INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY (app, name)
);
