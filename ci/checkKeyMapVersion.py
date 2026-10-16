# OSARA: Open Source Accessibility for the REAPER Application
# Check that key map changes update the key map version.
# Copyright 2026 James Teh
# License: GNU General Public License version 2.0

import argparse
import re
import subprocess
import sys


keyMapSource = "src/keyMap.cpp"
versionRe = re.compile(
	r"^[ \t]*constexpr[ \t]+int[ \t]+KEY_MAP_VERSION[ \t]*=[ \t]*(\d+)[ \t]*;[ \t]*$",
	re.MULTILINE,
)


def git(*args):
	result = subprocess.run(
		["git", *args], check=True, stdout=subprocess.PIPE, text=True
	)
	return result.stdout


def fileChanged(base, head, path):
	result = subprocess.run(
		["git", "diff", "--quiet", base, head, "--", path]
	)
	if result.returncode not in (0, 1):
		raise RuntimeError(f"Unable to compare {path} between {base} and {head}.")
	return result.returncode == 1


def getVersions(source):
	return [int(value) for value in versionRe.findall(source)]


def main():
	parser = argparse.ArgumentParser(
		description="Check that a changed key map has a newer key map version."
	)
	parser.add_argument("--base", required=True, help="Base commit for the pull request")
	parser.add_argument("--head", required=True, help="Head commit for the pull request")
	parser.add_argument("--key-map", dest="keyMap", required=True, help="Key map being checked")
	args = parser.parse_args()

	if not fileChanged(args.base, args.head, args.keyMap):
		print(f"{args.keyMap} did not change; no version update is required.")
		return 0

	baseVersions = getVersions(git("show", f"{args.base}:{keyMapSource}"))
	headVersions = getVersions(git("show", f"{args.head}:{keyMapSource}"))
	if not baseVersions or not headVersions:
		raise RuntimeError(
			f"Could not find KEY_MAP_VERSION in {keyMapSource} at both commits."
		)

	# The source may have one version for each platform. A unified version can
	# replace those conditional versions later, so compare against any previous
	# version rather than assuming there is exactly one assignment.
	if any(head > base for head in headVersions for base in baseVersions):
		print(
			f"{args.keyMap} changed and KEY_MAP_VERSION increased "
			f"({baseVersions} -> {headVersions})."
		)
		return 0

	print(
		f"::error file={keyMapSource}::"
		f"{args.keyMap} changed, but KEY_MAP_VERSION did not increase "
		f"({baseVersions} -> {headVersions})."
	)
	return 1


if __name__ == "__main__":
	try:
		sys.exit(main())
	except (RuntimeError, subprocess.CalledProcessError) as error:
		print(f"::error::{error}")
		sys.exit(1)
