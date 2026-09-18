# Project Ambrose by Imjustchico
# Front end of the client driver: it runs a scenario against the user's own client, says whether a run is possible, rebuilds the reference crops or lists the scenarios, and exits 0, 1 when the run failed, 2 on a refusal and 77 when this machine cannot run it.
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from clientdriver.cli import main

if __name__ == "__main__":
    sys.exit(main())
