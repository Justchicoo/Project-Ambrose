-- Project Ambrose by Imjustchico
-- Sealed storage for the panel settings that must never rest in the open: the SMTP password and the captcha secret. The keyring-sealed bytes and the id of the key that sealed them sit beside the plain value column, which stays empty for a sealed setting; rows written before this change keep their plain value until they are next saved.
ALTER TABLE panel_setting ADD COLUMN sealed_value TEXT;
ALTER TABLE panel_setting ADD COLUMN sealed_key_id INTEGER;
