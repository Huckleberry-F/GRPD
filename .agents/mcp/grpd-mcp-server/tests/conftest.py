import os
import sys


SERVER_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

if SERVER_ROOT not in sys.path:
    sys.path.insert(0, SERVER_ROOT)
