# OSARA: Open Source Accessibility for the REAPER Application
# Merge REAPER packages (.ReaperPkg) into one package.
# Copyright 2026 Leonard de Ruijter
# License: GNU General Public License version 2.0

import argparse
import sys
import zipfile


def merge(output, inputs):
	names = set()
	with zipfile.ZipFile(output, "w") as merged:
		for path in inputs:
			with zipfile.ZipFile(path) as package:
				for info in package.infolist():
					if info.filename in names:
						continue
					names.add(info.filename)
					merged.writestr(info, package.read(info), compresslevel=9)


def main():
	parser = argparse.ArgumentParser(
		description="Merge REAPER packages into one package. "
		"A file in more than one package is taken from the first package."
	)
	parser.add_argument("output", help="Package to write")
	parser.add_argument("inputs", nargs="+", help="Packages to merge")
	args = parser.parse_args()
	merge(args.output, args.inputs)
	return 0


if __name__ == "__main__":
	try:
		sys.exit(main())
	except (OSError, zipfile.BadZipFile) as error:
		print(f"::error::{error}")
		sys.exit(1)
