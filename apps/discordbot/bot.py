# Project Ambrose by Imjustchico
# The project's own Discord bot, run on one machine that keeps it online: setup clones main into the bot's folder, asks for the token without echoing it, installs the bot's packages and starts it at sign-in on Windows; run keeps the bot going and restarts it after a crash or when main changes its code; serve connects to Discord, answers the slash commands, keeps the progress and openings boards edited in place and posts merged pull requests, catching up on whatever changed while it was off.

import argparse
import asyncio
import getpass
import json
import logging
import logging.handlers
import os
import subprocess
import sys
import time
import urllib.error

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import boards

ROOT = os.path.dirname(os.path.dirname(HERE))
TOKEN_VARIABLE = "AMBROSE_DISCORD_TOKEN"
GITHUB_VARIABLE = "AMBROSE_GITHUB_TOKEN"
STARTUP = "Project Ambrose Discord bot"
RUN_KEY = r"Software\Microsoft\Windows\CurrentVersion\Run"
POLL_SECONDS = 300
PULLS_SECONDS = 120
UPDATED = 3
log = logging.getLogger("ambrose.bot")


def logging_to(home):
    os.makedirs(home.path, exist_ok=True)
    handler = logging.handlers.RotatingFileHandler(home.log, maxBytes=2_000_000, backupCount=3, encoding="utf-8")
    handler.setFormatter(logging.Formatter("%(asctime)s %(levelname)s %(name)s: %(message)s"))
    root = logging.getLogger()
    root.setLevel(logging.INFO)
    root.addHandler(handler)
    if sys.stderr is not None:
        root.addHandler(logging.StreamHandler())
    return handler


def preview(arguments):
    root = os.path.abspath(arguments.root)
    shown = {kind: boards.board_embed(kind, root) for kind in boards.BOARDS}
    if arguments.milestone:
        shown["milestone"] = boards.milestone_embed(root, arguments.milestone)
    print(json.dumps(shown, indent=2, ensure_ascii=False))
    return 0


def venv_python(home, windowed=False):
    if os.name == "nt":
        return os.path.join(home.venv, "Scripts", "pythonw.exe" if windowed else "python.exe")
    return os.path.join(home.venv, "bin", "python")


def setup(arguments):
    home = boards.Home(arguments.home)
    os.makedirs(home.path, exist_ok=True)
    print("cloned main into " + home.clone if boards.clone(home) else "the bot's clone of main is already there, bringing it up to date")
    boards.sync(home)

    token = None if arguments.new_token else boards.read_token(home.token, TOKEN_VARIABLE)
    while not token:
        token = getpass.getpass("Paste the bot token from the Discord developer portal (it is not shown): ").strip()
    try:
        me = boards.discord_get("/users/@me", token)
        application = boards.discord_get("/oauth2/applications/@me", token)
    except urllib.error.HTTPError as failure:
        print(f"Discord refused that token ({failure.code}), so nothing was saved; run setup again with --new-token", file=sys.stderr)
        return 1
    with open(home.token, "w", encoding="utf-8") as handle:
        handle.write(token)
    if os.name != "nt":
        os.chmod(home.token, 0o600)
    print(f"the token belongs to {me.get('username')}, saved in {home.token} and nowhere else")

    if not os.path.exists(venv_python(home)):
        subprocess.run([sys.executable, "-m", "venv", home.venv], check=True)
    requirements = os.path.join(home.clone, "apps", "discordbot", "requirements.txt")
    subprocess.run([venv_python(home), "-m", "pip", "install", "--quiet", "--disable-pip-version-check", "-r", requirements], check=True)
    print("installed the bot's packages into " + home.venv)

    script = os.path.join(home.clone, "apps", "discordbot", "bot.py")
    if os.name == "nt" and not arguments.no_autostart:
        import winreg
        launch = [venv_python(home, windowed=True), script, "run", "--home", home.path]
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, RUN_KEY, 0, winreg.KEY_SET_VALUE) as key:
            winreg.SetValueEx(key, STARTUP, 0, winreg.REG_SZ, subprocess.list2cmdline(launch))
        subprocess.Popen(launch, cwd=home.path, creationflags=subprocess.DETACHED_PROCESS | subprocess.CREATE_NEW_PROCESS_GROUP, close_fds=True)
        print(f'the bot now starts when you sign in to Windows, as "{STARTUP}" under your startup apps, and it is running now')
    else:
        print(f'start it with: "{venv_python(home)}" "{script}" run --home "{home.path}"')
    print("invite it to the server with this link, then use /board here in each channel it should keep a board in:")
    print(boards.invite_url(application["id"]))
    return 0


def only_one(home):
    handle = open(home.lock, "a+")
    try:
        if os.name == "nt":
            import msvcrt
            msvcrt.locking(handle.fileno(), msvcrt.LK_NBLCK, 1)
        else:
            import fcntl
            fcntl.flock(handle.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
    except OSError:
        handle.close()
        return None
    return handle


def run(arguments):
    home = boards.Home(arguments.home)
    logging_to(home)
    held = only_one(home)
    if held is None:
        log.info("the bot is already running from %s", home.path)
        return 0
    wait = 5
    while True:
        script = os.path.join(home.clone, "apps", "discordbot", "bot.py")
        if not os.path.exists(script):
            script = os.path.abspath(__file__)
        started = time.monotonic()
        code = subprocess.run([sys.executable, script, "serve", "--home", home.path]).returncode
        if code == 0:
            log.info("the bot stopped on request")
            return 0
        if code == UPDATED:
            log.info("main changed the bot's code, starting the new version")
            wait = 5
            continue
        if time.monotonic() - started > 600:
            wait = 5
        log.warning("the bot exited with %s, starting it again in %s seconds", code, wait)
        time.sleep(wait)
        wait = min(wait * 2, 300)


def serve(arguments):
    import discord
    from discord import app_commands

    home = boards.Home(arguments.home)
    logging_to(home)
    token = boards.read_token(home.token, TOKEN_VARIABLE)
    if not token:
        log.error("no bot token, so run setup first")
        return 2
    github_token = boards.read_token(home.github_token, GITHUB_VARIABLE)

    class Ambrose(discord.Client):
        def __init__(self):
            super().__init__(intents=discord.Intents.default())
            self.tree = app_commands.CommandTree(self)
            self.state = boards.load_state(home.state)
            self.lock = asyncio.Lock()
            self.exit_code = 1
            self.pulls_cache = (0.0, None)
            self.ids_cache = (None, [])

        def root(self):
            return home.clone if os.path.isdir(home.clone) else ROOT

        def save(self):
            boards.save_state(home.state, self.state)

        async def setup_hook(self):
            commands(self)
            self.loop.create_task(self.ticker())

        async def on_ready(self):
            log.info("connected as %s to %d server(s)", self.user, len(self.guilds))
            for joined in self.guilds:
                await self.sync_commands(joined)

        async def on_guild_join(self, joined):
            await self.sync_commands(joined)

        async def sync_commands(self, joined):
            try:
                self.tree.copy_global_to(guild=joined)
                await self.tree.sync(guild=joined)
            except discord.HTTPException as failure:
                log.warning("could not register commands in %s: %s", joined.name, failure)

        async def ticker(self):
            await self.wait_until_ready()
            while not self.is_closed():
                try:
                    await self.tick()
                except Exception:
                    log.exception("a round of updates failed")
                await asyncio.sleep(POLL_SECONDS)

        async def tick(self, force=False):
            async with self.lock:
                if os.path.isdir(os.path.join(home.clone, ".git")):
                    commit, code_changed = await asyncio.to_thread(boards.sync, home)
                    if code_changed:
                        self.exit_code = UPDATED
                        await self.close()
                        return
                else:
                    commit = boards.head(ROOT)
                if force or commit != self.state.get("commit"):
                    now = await asyncio.to_thread(boards.announce.load, self.root())
                    boards.advance(self.state, now)
                    for guild_id in list(self.state["guilds"]):
                        for kind in boards.BOARDS:
                            await self.update_board(guild_id, kind)
                    self.state["commit"] = commit
                    self.save()
                await self.post_merges()

        async def channel(self, channel_id):
            found = self.get_channel(int(channel_id))
            if found is None:
                found = await self.fetch_channel(int(channel_id))
            return found

        async def retire(self, entry, kind, channel):
            ids = await asyncio.to_thread(boards.retired_ids, self.root())
            for message_id in ids.get(kind, []):
                try:
                    old = await channel.fetch_message(message_id)
                    if old.webhook_id:
                        await old.delete()
                        log.info("deleted the old webhook %s message %s", kind, message_id)
                except discord.NotFound:
                    pass
                except discord.HTTPException as failure:
                    log.warning("could not delete the old webhook %s message %s: %s", kind, message_id, failure)
            entry["retired"][kind] = True

        async def update_board(self, guild_id, kind):
            entry = boards.guild(self.state, guild_id)
            channel_id = entry["channels"].get(kind)
            if not channel_id:
                return
            try:
                channel = await self.channel(channel_id)
                built = await asyncio.to_thread(boards.board_embed, kind, self.root(), self.state.get("progress_before"))
                embed = discord.Embed.from_dict(built)
                message_id = entry["messages"].get(kind)
                if message_id:
                    try:
                        message = await channel.fetch_message(int(message_id))
                        await message.edit(embed=embed)
                        return
                    except discord.NotFound:
                        log.info("the %s board message is gone, posting it again", kind)
                if not entry["retired"].get(kind):
                    await self.retire(entry, kind, channel)
                message = await channel.send(embed=embed)
                entry["messages"][kind] = str(message.id)
                self.save()
            except discord.HTTPException as failure:
                log.warning("could not update the %s board in %s: %s", kind, guild_id, failure)

        async def post_merges(self):
            targets = [(guild_id, entry["channels"]["merges"]) for guild_id, entry in self.state["guilds"].items() if entry.get("channels", {}).get("merges")]
            if not targets:
                return
            try:
                pulls = await asyncio.to_thread(boards.recent_merges, github_token)
            except (urllib.error.URLError, OSError, ValueError) as failure:
                log.warning("could not read merged pull requests: %s", failure)
                return
            since = self.state.get("merged_seen")
            fresh = boards.merged_since(pulls, since)
            if since is None:
                fresh = []
            for pull in fresh[-10:]:
                for guild_id, channel_id in targets:
                    try:
                        await (await self.channel(channel_id)).send(embed=discord.Embed.from_dict(boards.merged_embed(pull)))
                    except discord.HTTPException as failure:
                        log.warning("could not post merge %s in %s: %s", pull["number"], guild_id, failure)
            newest = boards.merged_since(pulls, None)
            if newest:
                self.state["merged_seen"] = max(since or "", newest[-1]["merged_at"])
                self.save()

        async def pulls(self):
            stamp, cached = self.pulls_cache
            if cached is None or time.monotonic() - stamp > PULLS_SECONDS:
                cached = await asyncio.to_thread(boards.open_pulls, github_token)
                self.pulls_cache = (time.monotonic(), cached)
            return cached

        async def ids(self):
            commit, cached = self.ids_cache
            now = boards.head(self.root())
            if commit != now:
                cached = await asyncio.to_thread(boards.milestone_ids, self.root())
                self.ids_cache = (now, cached)
            return cached

    def commands(bot):
        tree = bot.tree

        @tree.command(name="progress", description="How much of the plan is built")
        async def progress_command(interaction: discord.Interaction):
            built = await asyncio.to_thread(boards.progress_embed, bot.root(), bot.state.get("progress_before"))
            await interaction.response.send_message(embed=discord.Embed.from_dict(built))

        @tree.command(name="openings", description="Milestones ready to start, which anyone may take")
        async def openings_command(interaction: discord.Interaction):
            built = await asyncio.to_thread(boards.openings_embed, bot.root())
            await interaction.response.send_message(embed=discord.Embed.from_dict(built))

        @tree.command(name="milestone", description="One roadmap milestone: its size, checks and what it waits on")
        @app_commands.describe(id="The milestone's number, such as 6.07")
        async def milestone_command(interaction: discord.Interaction, id: str):
            built = await asyncio.to_thread(boards.milestone_embed, bot.root(), id)
            if built is None:
                await interaction.response.send_message(f"There is no milestone {id} on the roadmap.", ephemeral=True)
                return
            await interaction.response.send_message(embed=discord.Embed.from_dict(built))

        @milestone_command.autocomplete("id")
        async def milestone_choices(interaction: discord.Interaction, current: str):
            ids = await bot.ids()
            return [app_commands.Choice(name=value, value=value) for value in ids if value.startswith(current.strip())][:25]

        @tree.command(name="prs", description="Pull requests open right now")
        async def prs_command(interaction: discord.Interaction):
            await interaction.response.defer()
            try:
                built = boards.pulls_embed(await bot.pulls())
            except (urllib.error.URLError, OSError, ValueError):
                await interaction.followup.send("GitHub did not answer, so try again in a minute.")
                return
            await interaction.followup.send(embed=discord.Embed.from_dict(built))

        board = app_commands.Group(name="board", description="Choose where the bot keeps its boards",
                                   default_permissions=discord.Permissions(manage_guild=True), guild_only=True)
        kinds = [app_commands.Choice(name=name, value=name) for name in boards.KINDS]

        @board.command(name="here", description="Keep this board in this channel")
        @app_commands.choices(kind=kinds)
        async def board_here(interaction: discord.Interaction, kind: app_commands.Choice[str]):
            await interaction.response.defer(ephemeral=True)
            async with bot.lock:
                entry = boards.guild(bot.state, interaction.guild_id)
                if entry["channels"].get(kind.value) != str(interaction.channel_id):
                    entry["messages"].pop(kind.value, None)
                entry["channels"][kind.value] = str(interaction.channel_id)
                bot.save()
                if kind.value in boards.BOARDS:
                    if not bot.state.get("progress_now"):
                        boards.advance(bot.state, await asyncio.to_thread(boards.announce.load, bot.root()))
                    await bot.update_board(str(interaction.guild_id), kind.value)
                elif bot.state.get("merged_seen") is None:
                    bot.state["merged_seen"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
                bot.save()
            await interaction.followup.send(f"The {kind.value} board lives in this channel now.", ephemeral=True)

        @board.command(name="off", description="Stop keeping this board")
        @app_commands.choices(kind=kinds)
        async def board_off(interaction: discord.Interaction, kind: app_commands.Choice[str]):
            async with bot.lock:
                entry = boards.guild(bot.state, interaction.guild_id)
                entry["channels"].pop(kind.value, None)
                entry["messages"].pop(kind.value, None)
                bot.save()
            await interaction.response.send_message(f"The {kind.value} board is off. Its last message stays until someone deletes it.", ephemeral=True)

        @board.command(name="refresh", description="Bring main and every board up to date now")
        async def board_refresh(interaction: discord.Interaction):
            await interaction.response.defer(ephemeral=True)
            await bot.tick(force=True)
            await interaction.followup.send("Main and the boards are up to date.", ephemeral=True)

        tree.add_command(board)

    async def main():
        bot = Ambrose()
        async with bot:
            await bot.start(token)
        return bot.exit_code

    try:
        return asyncio.run(main())
    except KeyboardInterrupt:
        return 0
    except discord.LoginFailure:
        log.error("Discord refused the token, so run setup again with --new-token")
        return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description="Project Ambrose Discord bot")
    commands = parser.add_subparsers(dest="command", required=True)
    for name in ("setup", "run", "serve"):
        command = commands.add_parser(name)
        command.add_argument("--home", required=True, help="the bot's own folder, holding its clone of main, its token, state and log")
        if name == "setup":
            command.add_argument("--new-token", action="store_true", help="ask for the token again")
            command.add_argument("--no-autostart", action="store_true", help="do not start the bot at sign-in")
    shown = commands.add_parser("preview", help="print the boards the bot would post, with no Discord")
    shown.add_argument("--root", default=ROOT)
    shown.add_argument("--milestone")
    arguments = parser.parse_args(argv)
    return {"setup": setup, "run": run, "serve": serve, "preview": preview}[arguments.command](arguments)


if __name__ == "__main__":
    sys.exit(main())
