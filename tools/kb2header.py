import sortKeymap

def buildKeyList(fn):
	lines = open(fn, encoding='utf-8').readlines()
	sections = {
		0: [], 100:[], 102:[], 103:[],
		32060:[], 32061:[], 32062:[], 32063:[]}
		for line in lines:
			key, match = sortKeymap.parseLine(line)
			if key != 'KEY':
				continue
			section = int(match.group('section'))
			modifiers = int(match.group('modifiers'))
			key = int(match.group('key'))
			