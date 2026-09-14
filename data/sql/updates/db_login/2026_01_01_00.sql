-- Project Ambrose by Imjustchico
-- Adds the pending_db_login folder, where update files from open pull requests wait until they are merged, to the folders the updater reads.
INSERT IGNORE INTO `updates_include` (`path`, `state`) VALUES
    ('$/data/sql/updates/pending_db_login', 'PENDING');
