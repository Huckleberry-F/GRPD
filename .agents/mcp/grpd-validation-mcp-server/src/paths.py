import os


SERVER_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
DEFAULT_WORK_DIR = os.path.join(SERVER_ROOT, "work_dir")


def get_next_work_dir(base_dir: str = "", prefix: str = "run_") -> str:
    """Return the next numbered work directory under the validation service root."""
    root = os.path.abspath(base_dir or DEFAULT_WORK_DIR)
    os.makedirs(root, exist_ok=True)

    max_idx = 0
    for name in os.listdir(root):
        path = os.path.join(root, name)
        if not os.path.isdir(path) or not name.startswith(prefix):
            continue
        try:
            max_idx = max(max_idx, int(name[len(prefix):]))
        except ValueError:
            continue

    return os.path.join(root, f"{prefix}{max_idx + 1:04d}")
