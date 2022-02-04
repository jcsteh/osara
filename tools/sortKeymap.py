import re
import sys

regexs = {
	'ACT': re.compile(r'(ACT) (\d+) (\d+) ("[0-9a-f]+") (".*") (.*)$'),
	'SCR': re.compile(r'(SCR) (\d+) (\d+) ([^ ]+) (".*") (.*)$'),
	'KEY': re.compile(r'(KEY) (\d+) (\d+) ([^ ]+) (\d+)$')
}


def parseLine(line):
	for key,rx in regexs.items():
		match = rx.match(line)
		if(match):
			return key,match
	return None,None

acts=[]
scrs=[]
keys=[]

orig = open(sys.argv[1]).readlines()
for line in orig:
	key,match = parseLine(line)
	if match:
		if key == 'ACT':
			acts.append(match)
		elif key == 'SCR':
			scrs.append(match)
		elif key == 'KEY':
			keys.append(match)

from operator import methodcaller
acts.sort(key = lambda m: (m.group(3), m.group(5)))
scrs.sort(key = lambda m: (m.group(3), m.group(5)))
#sort keys by section, action id, modifiers and key.
keys.sort(key = lambda m: (m.group(5), m.group(4), m.group(2),m.group(3)))
for match in (acts+scrs+keys):
	if match:
		out = ' '.join(match.groups())
		print(out)
	else:
		raise Exception("failed to parse: "+out)
		