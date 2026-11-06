import argparse

from .server import serve

parser = argparse.ArgumentParser(prog="rigels_studio", description="Analyze and tune your reader's pacing.")
parser.add_argument("--port", type=int, default=8765)
parser.add_argument("--no-browser", action="store_true", help="don't open a browser window")
args = parser.parse_args()
serve(args.port, open_browser=not args.no_browser)
