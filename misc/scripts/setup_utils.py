#!/usr/bin/env python3

# A bunch of utility functions for the setup scripts

import sys
import subprocess
import urllib.request
import shutil
import zipfile

# added to all outputs for better clarity in the cmake output
LOG_PREFIX = "----"


# immediately flushes a message to stdout
def log(msg):
    print(f"{LOG_PREFIX} {msg}", flush=True)


# immediately flushes a message to stderr and exits with an errorcode
def fail(msg):
    print(f"{LOG_PREFIX} ERROR: {msg}", file=sys.stderr, flush=True)
    sys.exit(1)


# downloads a file from a url to a destination path with proper logging
def download(url, dest):
    log(f"Downloading {url}")
    try:
        with urllib.request.urlopen(url) as r, open(dest, "wb") as f:
            shutil.copyfileobj(r, f)
        log(f"Downloaded to {dest}")
    except Exception as e:
        fail(f"Failed to download {url}: {e}")


# extracts a zip file to a destination path with proper logging
def extract_zip(zip_path, extract_to):
    log(f"Extracting {zip_path}")
    try:
        with zipfile.ZipFile(zip_path, "r") as z:
            z.extractall(extract_to)
        log(f"Extracted to {extract_to}")
    except Exception as e:
        fail(f"Failed to extract {zip_path}: {e}")


# runs a command and logs the output with the LOG_PREFIX
def run_command(cmd, cwd=None, description=None):
    if description:
        log(description)

    cmd_str = ' '.join(cmd) if isinstance(cmd, list) else cmd
    log(f"Running: {cmd_str}" + (f" (cwd={cwd})" if cwd else ""))

    try:
        process = subprocess.Popen(
            cmd,
            cwd=cwd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            universal_newlines=True,
            bufsize=1
        )

        for line in process.stdout:
            print(f"{LOG_PREFIX} {line.rstrip()}", flush=True)

        return_code = process.wait()

        if return_code != 0:
            fail(f"Command failed with exit code {return_code}: {cmd_str}")

    except FileNotFoundError:
        fail(f"Command not found: {cmd[0]}")
    except Exception as e:
        fail(f"Failed to run command '{cmd_str}': {e}")
