# Project Ambrose by Imjustchico
# The scratch databases a run owns: it touches only names starting with ambrose_driver_, lets the server's own updater create them, loads the zone rows the game server stands wizards in, seeds the wizard a scenario enters the world with, its level, experience and any stats it carries, and reads its name back from the name tables the game server extracted, copies another database's wizard with its stats when that database has them, reads a value for an assertion, and drops them when the run ends.
import socket
import subprocess
import time

LOCALES = ("de", "el", "en-US", "es", "fr", "it", "pl", "pt-BR")
STATS = {"overflow_xp": "overflow_xp", "secondary_school": "secondary_school_id", "training_points": "training_points", "gold": "gold", "health": "health", "mana": "mana",
         "potion_charge": "potion_charge", "potion_max": "potion_max", "arena_points": "arena_points", "level_locked": "level_locked"}
APPEARANCE = ("behavior_template_name_id", "gender", "race", "head_hands_model", "hair_model", "hat_model", "torso_model", "feet_model",
              "wand_model", "skin_color", "skin_decal", "hair_color", "hat_color", "hat_decal", "torso_color", "torso_decal", "torso_decal2",
              "feet_color", "feet_decal", "skin_decal2", "extended_hair_color", "extended_skin_decal", "after_combat_dance",
              "after_combat_victory_dance", "new_player_options", "new_player_options2")

from .errors import Refused, StepFailed

PREFIX = "ambrose_driver_"
KINDS = ("login", "characters", "world")
NO_WINDOW = 0x08000000


def checked_name(name):
    if not name.startswith(PREFIX) or len(name) <= len(PREFIX):
        raise Refused(f"the driver may only use databases named {PREFIX}<something>, not {name!r}")
    if not all(character.isalnum() or character == "_" for character in name):
        raise Refused(f"{name!r} is not a plain database name")
    return name


class Scratch:
    def __init__(self, host, port, user, password, prefix=PREFIX + "run"):
        self.host = host
        self.port = int(port)
        self.user = user
        self.password = password
        self.names = {kind: checked_name(f"{prefix}_{kind}") for kind in KINDS}

    @property
    def address(self):
        return f"{self.host}:{self.port}"

    def info(self, kind):
        return f"{self.host};{self.port};{self.user};{self.password};{self.names[kind]}"

    def answers(self, timeout=2.0):
        try:
            with socket.create_connection((self.host, self.port), timeout):
                return True
        except OSError:
            return False

    def start_in_wsl(self, distribution, timeout=90):
        if self.answers():
            return "it was already answering"
        command = ["wsl.exe", "-d", distribution, "-u", "root", "--", "bash", "-c", "service mariadb start"]
        try:
            subprocess.run(command, capture_output=True, timeout=180, creationflags=NO_WINDOW)
        except (OSError, subprocess.SubprocessError) as error:
            raise StepFailed(f"MariaDB does not answer on {self.host}:{self.port} and WSL could not start it: {error}")
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if self.answers():
                return f"started in the WSL distribution {distribution}"
            time.sleep(1.0)
        raise StepFailed(f"MariaDB did not come up on {self.host}:{self.port} within {timeout}s")

    def _connect(self, database=None):
        import pymysql

        return pymysql.connect(host=self.host, port=self.port, user=self.user, password=self.password,
                               database=database, connect_timeout=10)

    def existing(self):
        connection = self._connect()
        try:
            with connection.cursor() as cursor:
                cursor.execute("SHOW DATABASES")
                return sorted(row[0] for row in cursor.fetchall() if row[0] in self.names.values())
        finally:
            connection.close()

    def drop(self):
        connection = self._connect()
        try:
            with connection.cursor() as cursor:
                for name in self.names.values():
                    cursor.execute(f"DROP DATABASE IF EXISTS `{checked_name(name)}`")
            connection.commit()
        finally:
            connection.close()
        return "dropped " + ", ".join(sorted(self.names.values()))

    def apply_sql(self, kind, path, batch=2000):
        connection = self._connect(self.names[kind])
        applied = 0
        try:
            with connection.cursor() as cursor, open(path, "r", encoding="utf-8") as handle:
                pending = []
                for line in handle:
                    line = line.strip()
                    if not line:
                        continue
                    pending.append(line)
                    if len(pending) >= batch:
                        applied += self._run(cursor, pending)
                        pending = []
                if pending:
                    applied += self._run(cursor, pending)
            connection.commit()
        finally:
            connection.close()
        return f"applied {applied} statement(s) from {path} to {self.names[kind]}"

    @staticmethod
    def _run(cursor, statements):
        for statement in statements:
            cursor.execute(statement)
        return len(statements)

    def account_id(self, user):
        found = self.value("login", f"SELECT id FROM account WHERE username = '{user}'")
        if found is None:
            raise StepFailed(f"the account {user} is not in {self.names['login']}")
        return int(found)

    def seed_character(self, user, wizard):
        stats = {STATS[key]: value for key, value in (wizard.get("stats") or {}).items()}
        account = self.account_id(user)
        locale = LOCALES.index(wizard.get("locale", "en-US")) + 1
        indices = (locale << 24) | (int(wizard["first"]) << 16) | (int(wizard["middle"]) << 8) | int(wizard["last"])
        look = dict.fromkeys(APPEARANCE, 0)
        look.update({key: int(value) for key, value in (wizard.get("appearance") or {}).items() if key in look})
        connection = self._connect(self.names["characters"])
        try:
            with connection.cursor() as cursor:
                cursor.execute("SELECT COALESCE(MAX(guid), 0) + 1 FROM characters")
                guid = int(cursor.fetchone()[0])
                cursor.execute("INSERT INTO characters (guid, account, name_indices, school_id, level, xp, world, zone, zone_display, created) "
                               "VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, UNIX_TIMESTAMP())",
                               (guid, account, indices, int(wizard["school"]), int(wizard.get("level", 1)), int(wizard.get("experience", 0)),
                                int(wizard.get("world", 0)), wizard["zone"], wizard.get("zone_display", "")))
                if stats:
                    cursor.execute(f"INSERT INTO character_stats (guid, {', '.join(stats)}) VALUES (%s{', %s' * len(stats)})", [guid] + list(stats.values()))
                cursor.execute(f"INSERT INTO character_appearance (guid, {', '.join(APPEARANCE)}) VALUES (%s{', %s' * len(APPEARANCE)})",
                               [guid] + [look[key] for key in APPEARANCE])
                cursor.execute("INSERT INTO id_sequences (name, highest) VALUES ('character', %s) ON DUPLICATE KEY UPDATE highest = GREATEST(highest, VALUES(highest))",
                               (guid,))
            connection.commit()
        finally:
            connection.close()
        name = self.character_name(wizard, look["gender"])
        return guid, name

    @staticmethod
    def read_wizard(info, guid):
        import pymysql

        host, port, user, password, database = info.split(";")
        connection = pymysql.connect(host=host, port=int(port), user=user, password=password, database=database, connect_timeout=10)
        try:
            with connection.cursor(pymysql.cursors.DictCursor) as cursor:
                cursor.execute("SELECT name_indices, school_id, level, xp, world, zone, zone_display FROM characters WHERE guid = %s", (int(guid),))
                row = cursor.fetchone()
                if row is None:
                    raise StepFailed(f"wizard {guid} is not in {database}")
                cursor.execute(f"SELECT {', '.join(APPEARANCE)} FROM character_appearance WHERE guid = %s", (int(guid),))
                look = cursor.fetchone() or {}
                cursor.execute("SELECT COUNT(*) AS found FROM information_schema.tables WHERE table_schema = DATABASE() AND table_name = 'character_stats'")
                stats = None
                if int(cursor.fetchone()["found"]):
                    cursor.execute(f"SELECT {', '.join(STATS.values())} FROM character_stats WHERE guid = %s", (int(guid),))
                    stats = cursor.fetchone()
        finally:
            connection.close()
        return Scratch.wizard_from_rows(row, look, stats)

    @staticmethod
    def wizard_from_rows(row, look, stats=None):
        indices = int(row["name_indices"])
        locale = (indices >> 24) & 0xFF
        wizard = {"school": int(row["school_id"]), "level": int(row["level"]), "experience": int(row.get("xp", 0)), "world": int(row["world"]), "zone": row["zone"],
                  "zone_display": row["zone_display"], "locale": LOCALES[locale - 1] if 0 < locale <= len(LOCALES) else "en-US",
                  "first": (indices >> 16) & 0xFF, "middle": (indices >> 8) & 0xFF, "last": indices & 0xFF,
                  "appearance": {key: int(value) for key, value in look.items() if key in APPEARANCE}}
        if stats:
            columns = {column: key for key, column in STATS.items()}
            wizard["stats"] = {columns[column]: value for column, value in stats.items() if column in columns and value is not None}
        return wizard

    def character_name(self, wizard, gender):
        locale = wizard.get("locale", "en-US")
        first_table = "FirstName_HumanMale" if gender == 1 else "FirstName_HumanFemale"
        parts = []
        for table, index in ((first_table, wizard["first"]), ("MiddleName_Human", wizard["middle"]), ("LastName_Human", wizard["last"])):
            text = self.value("world", f"SELECT text FROM character_name_part WHERE table_name = '{table}' AND locale = '{locale}' AND idx = {int(index)}")
            if text is None:
                raise StepFailed(f"{table} {index} is not in the {locale} name tables of {self.names['world']}")
            parts.append(text)
        return f"{parts[0]} {parts[1]}{parts[2]}".strip()

    def value(self, kind, query):
        if kind not in self.names:
            raise Refused(f"the driver has no {kind} database; it has {', '.join(self.names)}")
        connection = self._connect(self.names[kind])
        try:
            with connection.cursor() as cursor:
                cursor.execute(query)
                row = cursor.fetchone()
                return None if row is None else row[0]
        finally:
            connection.close()
