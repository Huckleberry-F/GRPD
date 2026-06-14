import os
import sys


SERVER_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SRC_ROOT = os.path.join(SERVER_ROOT, "src")

for path in (SERVER_ROOT, SRC_ROOT):
    if path not in sys.path:
        sys.path.insert(0, path)
