"""Add the game to Steam as a non-Steam game, with its library pictures (setup.py: "Add to Steam").

Steam keeps a user's non-Steam games in userdata/<account>/config/shortcuts.vdf (a binary key-value file) and their
pictures in userdata/<account>/config/grid/ under the shortcut's app id:
    <appid>p.png       the cover (portrait capsule, 600x900)
    <appid>.png        the wide cover (horizontal capsule)
    <appid>_hero.png   the background at the top of the game's page
    <appid>_logo.png   the logo drawn over it
The icon is a path in the shortcut itself. Steam reads shortcuts.vdf at start and writes it back when it exits, so it
must not be running while the file changes (running(), shutdown()).

Only standard library; tested by tools/installer/test_setup.py (Steam*).
"""
import os
import re
import shutil
import struct
import subprocess
import sys
import time
import zlib

IS_WIN = sys.platform.startswith("win")
# pictures: the release's steam/ folder (tools/release/package.py copies assets/steam there)
PICTURES = {"cover": "cover.png", "wide": "wide.png", "hero": "hero.png", "logo": "logo.png"}
GRID_NAMES = {"cover": "%dp", "wide": "%d", "hero": "%d_hero", "logo": "%d_logo"}
STEAM_ID64_BASE = 76561197960265728


class SteamError(Exception):
    pass


# ---------------------------------------------------------------------------------------------
# binary VDF (shortcuts.vdf): a map is a list of (type, key, value); 0x00 map, 0x01 string, 0x02 int32, 0x08 end.
# Other types (0x03 float, 0x07 uint64, ...) are kept as raw bytes so a file Steam wrote is written back unchanged.

class Raw:
    def __init__(self, typ, data):
        self.typ, self.data = typ, data

    def __eq__(self, other):
        return isinstance(other, Raw) and (self.typ, self.data) == (other.typ, other.data)


RAW_SIZES = {0x03: 4, 0x04: 4, 0x05: 4, 0x06: 4, 0x07: 8, 0x0A: 8}


def _cstr(data, pos):
    end = data.index(b"\x00", pos)
    return data[pos:end].decode("utf-8", "replace"), end + 1


def parse_vdf(data):
    """bytes -> dict (insertion order kept). Raises SteamError on a damaged file."""
    def read_map(pos):
        out = {}
        while True:
            if pos >= len(data):
                raise SteamError("shortcuts.vdf ends early")
            typ = data[pos]
            pos += 1
            if typ == 0x08:
                return out, pos
            key, pos = _cstr(data, pos)
            if typ == 0x00:
                out[key], pos = read_map(pos)
            elif typ == 0x01:
                out[key], pos = _cstr(data, pos)
            elif typ == 0x02:
                out[key] = struct.unpack_from("<i", data, pos)[0]
                pos += 4
            elif typ in RAW_SIZES:
                out[key] = Raw(typ, data[pos:pos + RAW_SIZES[typ]])
                pos += RAW_SIZES[typ]
            else:
                raise SteamError("shortcuts.vdf has an unknown value type 0x%02x" % typ)
    if not data:
        return {"shortcuts": {}}
    try:
        root, pos = read_map(0)
    except (ValueError, struct.error) as e:
        raise SteamError("shortcuts.vdf is damaged (%s)" % e)
    return root


def write_vdf(root):
    out = bytearray()

    def write_map(m):
        for key, v in m.items():
            k = key.encode("utf-8") + b"\x00"
            if isinstance(v, dict):
                out.extend(b"\x00" + k)
                write_map(v)
            elif isinstance(v, str):
                out.extend(b"\x01" + k + v.encode("utf-8") + b"\x00")
            elif isinstance(v, bool) or isinstance(v, int):
                out.extend(b"\x02" + k + struct.pack("<i", int(v)))
            elif isinstance(v, Raw):
                out.extend(bytes([v.typ]) + k + v.data)
            else:
                raise TypeError("unsupported VDF value %r" % (v,))
        out.append(0x08)
    write_map(root)
    return bytes(out)


# ---------------------------------------------------------------------------------------------
# where Steam is

def steam_roots():
    """Steam installations that have user data, most likely first."""
    cands = []
    if IS_WIN:
        try:
            import winreg
            with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Software\Valve\Steam") as k:
                cands.append(winreg.QueryValueEx(k, "SteamPath")[0])
        except OSError:
            pass
        for env in ("ProgramFiles(x86)", "ProgramFiles"):
            if os.environ.get(env):
                cands.append(os.path.join(os.environ[env], "Steam"))
    else:
        home = os.path.expanduser("~")
        data = os.environ.get("XDG_DATA_HOME") or os.path.join(home, ".local", "share")
        cands += [os.path.join(data, "Steam"), os.path.join(home, ".steam", "steam"), os.path.join(home, ".steam", "root"),
                  os.path.join(home, ".var", "app", "com.valvesoftware.Steam", ".local", "share", "Steam")]
    seen, out = set(), []
    for c in cands:
        c = os.path.normpath(c.replace("/", os.sep))
        real = os.path.realpath(c)
        if real in seen or not os.path.isdir(os.path.join(c, "userdata")):
            continue
        seen.add(real)
        out.append(c)
    return out


def is_flatpak(root):
    return os.sep + os.path.join(".var", "app", "com.valvesoftware.Steam") + os.sep in os.path.realpath(root) + os.sep


def users(root):
    """Account ids (userdata folder names) to add the game for: the one that logged in last, else every account."""
    ud = os.path.join(root, "userdata")
    ids = sorted(d for d in os.listdir(ud) if d.isdigit() and d != "0" and os.path.isdir(os.path.join(ud, d)))
    try:
        with open(os.path.join(root, "config", "loginusers.vdf"), encoding="utf-8", errors="replace") as f:
            text = f.read()
        # text VDF: "7656119..." { ... "MostRecent" "1" ... }
        for sid, body in re.findall(r'"(\d{17})"\s*\{([^{}]*)\}', text):
            if re.search(r'"(?i:mostrecent)"\s*"1"', body):
                acc = str(int(sid) - STEAM_ID64_BASE)
                if acc in ids:
                    return [acc]
    except OSError:
        pass
    return ids


# ---------------------------------------------------------------------------------------------
# Steam running?

def running():
    """True while a Steam client runs (it would overwrite shortcuts.vdf when it exits)."""
    if IS_WIN:
        try:
            out = subprocess.run(["tasklist", "/FI", "IMAGENAME eq steam.exe", "/NH"], capture_output=True, text=True,
                                 creationflags=0x08000000).stdout  # CREATE_NO_WINDOW
        except OSError:
            return False
        return "steam.exe" in out.lower()
    try:
        pids = [p for p in os.listdir("/proc") if p.isdigit()]
    except OSError:
        return False
    for p in pids:
        try:
            with open("/proc/%s/comm" % p) as f:
                if f.read().strip() == "steam":
                    return True
        except OSError:
            continue
    return False


def steam_command(root):
    """How to start Steam (for -shutdown and to start it again), or None."""
    if IS_WIN:
        exe = os.path.join(root, "steam.exe")
        return [exe] if os.path.isfile(exe) else None
    if is_flatpak(root) and shutil.which("flatpak"):
        return ["flatpak", "run", "com.valvesoftware.Steam"]
    steam = shutil.which("steam")
    return [steam] if steam else None


def shutdown(root, timeout=60):
    """Asks Steam to quit and waits for it. Returns True when it is no longer running."""
    cmd = steam_command(root)
    if not cmd:
        raise SteamError("Steam is running and could not be asked to quit: close Steam, then try again.")
    subprocess.Popen(cmd + ["-shutdown"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=not IS_WIN)
    end = time.time() + timeout
    while time.time() < end:
        time.sleep(1)
        if not running():
            time.sleep(2)  # it writes its files as it exits
            return True
    return False


def start(root):
    cmd = steam_command(root)
    if cmd:
        kw = {"creationflags": 0x00000008 | 0x00000200} if IS_WIN else {"start_new_session": True}
        subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, **kw)


# ---------------------------------------------------------------------------------------------
# the shortcut

def quoted(path):
    return '"%s"' % path


def unquoted(s):
    s = s.strip()
    return s[1:-1] if len(s) >= 2 and s[0] == s[-1] == '"' else s


def same_path(a, b):
    a, b = os.path.normpath(unquoted(a)), os.path.normpath(unquoted(b))
    return a.lower() == b.lower() if IS_WIN else a == b


def shortcut_appid(exe_field, name):
    """The id Steam gives a non-Steam game (its pictures are named by it): CRC-32 of the Exe field (as stored,
    quoted) and the name, with the top bit set. Returned unsigned; shortcuts.vdf stores it as a signed int32."""
    return (zlib.crc32((exe_field + name).encode("utf-8")) & 0xFFFFFFFF) | 0x80000000


def signed(appid):
    return appid - (1 << 32) if appid >= 1 << 31 else appid


def find_shortcut(shortcuts, exe):
    for key, entry in shortcuts.items():
        if isinstance(entry, dict) and same_path(entry.get("Exe", entry.get("exe", "")), exe):
            return key
    return None


def add_shortcut(root_map, name, exe, start_dir, launch_options, icon):
    """Adds the game to a parsed shortcuts.vdf, or updates the entry that already starts the same program (its name,
    a name you gave it in Steam, is kept). Returns (appid unsigned, added)."""
    sc = root_map.setdefault("shortcuts", {})
    key = find_shortcut(sc, exe)
    if key is not None:
        e = sc[key]
        exe_field = e.get("Exe", quoted(exe))
        appid = e.get("appid")
        appid = (appid & 0xFFFFFFFF) if isinstance(appid, int) else shortcut_appid(exe_field, e.get("AppName", name))
        e["appid"] = signed(appid)
        e["StartDir"] = quoted(start_dir)
        e["icon"] = icon
        if launch_options and not e.get("LaunchOptions"):
            e["LaunchOptions"] = launch_options
        return appid, False
    exe_field = quoted(exe)
    appid = shortcut_appid(exe_field, name)
    index = str(max([int(k) for k in sc if k.isdigit()] + [-1]) + 1)
    sc[index] = {
        "appid": signed(appid), "AppName": name, "Exe": exe_field, "StartDir": quoted(start_dir), "icon": icon,
        "ShortcutPath": "", "LaunchOptions": launch_options, "IsHidden": 0, "AllowDesktopConfig": 1,
        "AllowOverlay": 1, "OpenVR": 0, "Devkit": 0, "DevkitGameID": "", "DevkitOverrideAppID": 0, "LastPlayTime": 0,
        "FlatpakAppID": "", "tags": {},
    }
    return appid, True


def copy_pictures(pictures_dir, grid_dir, appid):
    """The release's pictures into Steam's grid folder under the shortcut's id (replacing earlier ones). Returns how
    many were copied."""
    os.makedirs(grid_dir, exist_ok=True)
    n = 0
    for kind, src_name in PICTURES.items():
        src = os.path.join(pictures_dir, src_name)
        if not os.path.isfile(src):
            continue
        base = GRID_NAMES[kind] % appid
        for ext in (".png", ".jpg", ".jpeg", ".webp"):  # an older picture of the same kind would win otherwise
            old = os.path.join(grid_dir, base + ext)
            if os.path.isfile(old):
                os.remove(old)
        shutil.copyfile(src, os.path.join(grid_dir, base + os.path.splitext(src_name)[1]))
        n += 1
    return n


def add(name, exe, start_dir, launch_options, pictures_dir, icon_dir, log=lambda s: None, root=None):
    """Adds (or updates) the game for the Steam account that logged in last. Steam must not be running.
    icon_dir: a folder that outlives this setup (the data folder) for the shortcut's icon.
    Returns a list of (account, appid, added)."""
    roots = [root] if root else steam_roots()
    if not roots:
        raise SteamError("Steam was not found on this computer.")
    root = roots[0]
    if running():
        raise SteamError("running")
    accounts = users(root)
    if not accounts:
        raise SteamError("Steam has no user yet: start Steam once and sign in, then try again.")
    icon_src = os.path.join(pictures_dir, "icon.ico" if IS_WIN else "icon.png")
    icon = ""
    if os.path.isfile(icon_src):
        os.makedirs(icon_dir, exist_ok=True)
        icon = os.path.join(icon_dir, "steam-icon" + os.path.splitext(icon_src)[1])
        shutil.copyfile(icon_src, icon)
    done = []
    for acc in accounts:
        cfg = os.path.join(root, "userdata", acc, "config")
        os.makedirs(cfg, exist_ok=True)
        path = os.path.join(cfg, "shortcuts.vdf")
        data = b""
        if os.path.isfile(path):
            with open(path, "rb") as f:
                data = f.read()
            backup = path + ".tphd-backup"
            if not os.path.exists(backup):  # the state before setup first changed it
                shutil.copyfile(path, backup)
        root_map = parse_vdf(data)
        appid, added = add_shortcut(root_map, name, exe, start_dir, launch_options, icon)
        tmp = path + ".tmp"
        with open(tmp, "wb") as f:
            f.write(write_vdf(root_map))
        os.replace(tmp, path)
        pics = copy_pictures(pictures_dir, os.path.join(cfg, "grid"), appid)
        log("Steam account %s: %s %s (app id %d), %d pictures" % (acc, "added" if added else "updated", name, appid, pics))
        done.append((acc, appid, added))
    return done
