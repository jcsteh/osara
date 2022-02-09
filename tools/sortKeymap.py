import sys
import re
import os

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
			raise Exception("Unable to parse line: "+line)

	acts.sort(key = lambda m: (m.group(3), m.group(5)))
	scrs.sort(key = lambda m: (m.group(3), m.group(5)))
	#sort keys by section, action id, modifiers and key.
	keys.sort(key = lambda m: (m.group(5), m.group(4), m.group(2),m.group(3)))
	return [match.group(0) for match in (acts+scrs+keys)]

def main():
	fn = sys.argv[1]
	orig = open(fn, encoding='utf-8').readlines()
	os.replace(fn, fn+'.bak')
	try:
		sorted = sortKeyMap(orig)
		with open(fn, 'x', encoding='utf-8') as outFile:
			outFile.write('\n'.join(sorted))
			outFile.write('\n')
	except Exception as e:
		os.replace(fn+'.bak', fn)
		print(e)
		return 1

if __name__ == "__main__":
	exitCode = main()
	sys.exit(exitCode)
