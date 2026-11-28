"""Project Hub domain: creation, safe sample copying and recent projects.

The existing recipe generator and native editor remain the format authorities.
No project mutation is performed when listing or opening a recent descriptor.
"""
from dataclasses import dataclass
import json
import os
from pathlib import Path
import re
import shutil
import tempfile
import sys

ROOT = Path(__file__).resolve().parents[1]
LIMIT = 32
MAX_RECENT_BYTES = 256 * 1024
MAX_COPY_BYTES = 512 * 1024 * 1024
MAX_COPY_FILES = 10000


@dataclass(frozen=True)
class Template:
    key: str
    title: str
    description: str
    sample: str = ""
    layout: str = ""
    available: bool = True


TEMPLATES = (
    Template("empty", "3D Starter", "A minimal playable world with player, camera, lighting and boundaries.", layout="empty"),
    Template("arena", "3D Arena", "A playable 3D level with ready-made scenery and controls.", layout="arena"),
    Template("beacon", "Collectible Game", "Collect six beacons and complete the objective.", sample="BeaconRun"),
    Template("keygate", "Adventure Starter", "Collect a key to open a physical gate.", sample="KeyGateArena"),
    Template("2d", "2D Platformer", "Editable tile map, collectibles, jumping, hazards and goal.", sample="PlatformerProject"),
    Template("world2d", "2D Adventure", "Scrolling world, tile layers, animated actors, top-down or platform movement.", sample="World2DProject"),
    Template("physics2d", "2D Physics Playground", "Dynamic props, gravity and collision-material authoring.", sample="PhysicsWorld2DProject"),
)


def project_name(value):
    # Portable names also serve as one directory component. Reject device
    # names, dot segments, shell metacharacters and leading/trailing spaces.
    if not isinstance(value, str) or not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9 _-]{0,63}", value) or value.endswith(" "):
        raise ValueError("Use 1–64 letters, digits, spaces, hyphens or underscores; start with a letter or digit.")
    if re.fullmatch(r"(?i:con|prn|aux|nul|com[1-9]|lpt[1-9])", value):
        raise ValueError("This name is reserved by Windows.")
    return value


def descriptor(path):
    p = Path(path).expanduser().absolute()
    if p.suffix != ".kproject" or p.is_symlink() or not p.is_file():
        raise ValueError("Choose an existing regular .kproject file.")
    return p.resolve(strict=True)


def default_recent_path():
    if os.name == "nt":
        base = Path(os.environ.get("LOCALAPPDATA", Path.home() / "AppData/Local"))
    elif os.sys.platform == "darwin":
        base = Path.home() / "Library/Application Support"
    else:
        base = Path(os.environ.get("XDG_STATE_HOME", Path.home() / ".local/state"))
    return base / "Kairo" / "hub-recent-v1.json"


class RecentProjects:
    def __init__(self, path):
        self.path = Path(path)
        self.paths = []
        self.warning = ""
        if self.path.exists():
            try:
                with self.path.open("rb") as stream:
                    raw = stream.read(MAX_RECENT_BYTES + 1)
                if len(raw) > MAX_RECENT_BYTES:
                    raise ValueError("Recent list exceeds size limit.")
                data = json.loads(raw)
                if not isinstance(data, dict) or data.get("version") != 1 or not isinstance(data.get("projects"), list):
                    raise ValueError("Unrecognized recent list.")
                for value in data["projects"][:LIMIT]:
                    if not isinstance(value, str) or len(value) > 4096 or "\0" in value or not Path(value).is_absolute():
                        raise ValueError("Invalid saved project path.")
                    if value not in self.paths:
                        self.paths.append(value)
            except (OSError, ValueError, UnicodeError) as error:
                # Refuse to overwrite a malformed file until explicitly reset.
                self.paths = []
                self.warning = f"Recent list could not be loaded: {error}"

    def _save(self, paths):
        if self.warning:
            raise ValueError(self.warning)
        payload = json.dumps({"version": 1, "projects": paths}, ensure_ascii=False).encode("utf-8")
        if len(payload) > MAX_RECENT_BYTES:
            raise ValueError("Recent list exceeds size limit.")
        self.path.parent.mkdir(parents=True, exist_ok=True)
        fd, temporary = tempfile.mkstemp(prefix=".hub-recent-", dir=self.path.parent)
        try:
            with os.fdopen(fd, "wb") as stream:
                stream.write(payload)
                stream.flush()
                os.fsync(stream.fileno())
            os.replace(temporary, self.path)
        finally:
            Path(temporary).unlink(missing_ok=True)
        self.paths = paths

    def touch(self, path):
        value = str(descriptor(path))
        if len(value) > 4096:
            raise ValueError("Project path exceeds recent-list limit.")
        self._save(([value] + [p for p in self.paths if os.path.normcase(p) != os.path.normcase(value)])[:LIMIT])

    def remove(self, value):
        self._save([p for p in self.paths if p != value])

    def recover(self):
        """Explicit recovery retains the original malformed bytes."""
        if not self.warning:
            raise ValueError("The recent list does not require recovery.")
        fd, name = tempfile.mkstemp(prefix="hub-recent-invalid-", suffix=".bak", dir=self.path.parent)
        os.close(fd)
        backup = Path(name)
        try:
            os.replace(self.path, backup)
        except OSError:
            backup.unlink(missing_ok=True)
            raise
        self.warning = ""
        self.paths = []
        return backup


def sample_files(source):
    """Return only regular authored files; reject links and bound total bytes.

    The trusted engine sample tree may contain local runtime output. Exclude
    those directories at every level, never follow symlinks, and copy licenses.
    """
    skip = {".git", ".kairo", "build", "__pycache__", ".ds_store"}
    files, total = [], 0
    if source.is_symlink() or not source.is_dir():
        raise ValueError("Sample directory missing or linked.")
    for root, dirs, names in os.walk(source, followlinks=False):
        here = Path(root)
        dirs[:] = sorted(d for d in dirs if d.lower() not in skip)
        for d in dirs:
            if (here / d).is_symlink():
                raise ValueError("Sample contains a linked directory.")
        for filename in sorted(names):
            if filename.lower() in skip or filename.endswith((".pyc", ".kprogress")):
                continue
            p = here / filename
            if p.is_symlink() or not p.is_file():
                raise ValueError("Sample contains a non-regular file.")
            total += p.stat().st_size
            files.append(p)
            if len(files) > MAX_COPY_FILES or total > MAX_COPY_BYTES:
                raise ValueError("Sample exceeds copy budget.")
    return files


def create(template_key, name, parent, root=ROOT):
    name = project_name(name)
    template = next((t for t in TEMPLATES if t.key == template_key and t.available), None)
    if template is None:
        raise ValueError("This project template is unavailable.")
    parent = Path(parent).expanduser().resolve(strict=True)
    if not parent.is_dir():
        raise ValueError("Choose an existing projects directory.")
    output = parent / name
    if template.layout:
        from create_kairo_project import create_project
        with tempfile.TemporaryDirectory(prefix="kairo-hub-") as temporary:
            recipe = Path(temporary) / "recipe.json"
            recipe.write_text(json.dumps({"name": name, "layout": template.layout,
                                          "prefabs": {}, "instances": []}), encoding="utf-8")
            return descriptor(create_project(recipe, output, Path(root) / "Samples/PlayableVerticalSlice"))
    source = Path(root) / "Samples" / template.sample
    if source.is_symlink():
        raise ValueError("Sample directory is linked.")
    source = source.resolve(strict=True)
    if source.is_relative_to(output) or output.is_relative_to(source):
        raise ValueError("A sample must be copied outside its own source tree.")
    files = sample_files(source)
    original = descriptor(source / "Project.kproject")
    if original.stat().st_size > 1024 * 1024:
        raise ValueError("Sample project descriptor exceeds size limit.")
    text = original.read_text(encoding="utf-8")
    renamed, count = re.subn(r'(?m)^name .+$', lambda _: 'name "' + name + '"', text)
    if count != 1:
        raise ValueError("Unexpected sample project descriptor.")
    output.mkdir()  # Exclusive reservation, never overwrite an existing project.
    try:
        for file in files:
            target = output / file.relative_to(source)
            target.parent.mkdir(parents=True, exist_ok=True)
            with target.open("xb") as stream:
                if file == original:
                    stream.write(renamed.encode("utf-8"))
                else:
                    with file.open("rb") as data:
                        shutil.copyfileobj(data, stream)
        return descriptor(output / "Project.kproject")
    except BaseException:
        shutil.rmtree(output)
        raise


def editor_command(path, build, renderer="auto", root=ROOT):
    from kairo_project_workflow import ProjectTools
    tools = ProjectTools(descriptor(path), build, root)
    return tools.commands("editor", renderer)[0]


def workbench_command(path, build, renderer="auto", root=ROOT):
    """Open the existing project workflow with the same project and renderer."""
    from kairo_project_workflow import ProjectTools
    tools = ProjectTools(descriptor(path), build, root)
    if renderer not in ("auto", "metal", "vulkan", "opengl", "d3d12"):
        raise ValueError("Unknown renderer")
    script = tools.root / "scripts/kairo_project_workflow.py"
    if not script.is_file():
        raise ValueError("Project Workbench script missing")
    return [sys.executable, "-B", str(script), str(tools.project),
            "--build", str(tools.build), "--renderer", renderer]
