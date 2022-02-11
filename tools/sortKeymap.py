# OSARA: Open Source Accessibility for the REAPER Application
# Keymap sorting utility
# Copyright 2022 Robbie Murray
# License: GNU General Public License version 2.0

import sys
import re
import os

regexs = {
	'ACT': re.compile(r'(ACT) (?P<options>\d+) (?P<section>\d+) (?P<actionId>"[0-9a-f]+") (?P<description>".*") (.*)$'),
	'SCR': re.compile(r'(SCR) (?P<options>\d+) (?P<section>\d+) (?P<actionId>[^ ]+) (?P<description>".*") (?P<fileName>.*)$'),
	'KEY': re.compile(r'(KEY) (?P<modifiers>\d+) (?P<key>\d+) (?P<actionId>[^ ]+) (?P<section>\d+)$')
}


def parseLine(line):
	for key,rx in regexs.items():
		match = rx.match(line)
		if(match):
			return key,match
	return None,None

def sortKeyMap(lines):
	acts=[]
	scrs=[]
	keys=[]
	for line in lines:
		key,match = parseLine(line)
		if match:
			if key == 'ACT':
				acts.append(match)
			elif key == 'SCR':
				scrs.append(match)
			elif key == 'KEY':
				keys.append(match)
		else:
			raise Exception(f"Unable to parse line: {line}")

	acts.sort(key = lambda m: (m.group('section'), m.group('description')))
	scrs.sort(key = lambda m: (m.group('section'), m.group('description')))
	keys.sort(key = lambda m: (m.group('section'), m.group('actionId'), m.group('modifiers'),m.group('key')))
	return [match.group(0) + '\n' for match in (acts+scrs+keys)]

def testFile(fn):
	orig = open(fn, encoding='utf-8').readlines()
	sorted = sortKeyMap(orig)
	if(sorted == orig):
		print(f'Keymap {fn} is already sorted')
		return True
	else:
		print(f'Keymap {fn} not sorted')
		return False

def main(args):
	if(args.test):
		if(not testFile(args.file)):
			return 1
	fn = args.file
	if(args.output):
		outFn = args.output
	else:
		outFn = args.file
	orig = open(fn, encoding='utf-8').readlines()
	if(outFn == fn):
		os.replace(fn, fn + '.bak')
	try:
		sorted = sortKeyMap(orig)
		with open(outFn, 'x', encoding='utf-8') as outFile:
			outFile.writelines(sorted)
		return 0
	except Exception as e:
		if(outFn == fn):
			os.replace(fn+'.bak', fn)
		print(e)
		return 1

if __name__ == "__main__":
	import argparse
	parser = argparse.ArgumentParser(description = "Sort a Reaper keymap file")
	parser.add_argument('file',
		help = 'the keymap file to be sorted')
	group = parser.add_mutually_exclusive_group()
	group.add_argument('-o', '--output', 
		help = 'Specify the output file. Defaults to overwriting the input file.')
	group.add_argument('-t', '--test',
		help = 'check if the file has already been sorted. Exit with status 1 if not..',
		action = 'store_true')
	args = parser.parse_args()
	exitCode = main(args)
	sys.exit(exitCode)
