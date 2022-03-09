import sortKeymap

def buildKeyList(fn):
	lines = open(fn, encoding='utf-8').readlines()
	numericActions = {
		0:[], 100:[], 102:[], 103:[],
		32060:[], 32061:[], 32062:[], 32063:[]}
	customActions = {
		0:[], 100:[], 102:[], 103:[],
		32060:[], 32061:[], 32062:[], 32063:[]}
	for line in lines:
		key, match = sortKeymap.parseLine(line)
		if key != 'KEY':
			continue
		section = int(match.group('section'))
		modifiers = int(match.group('modifiers'))
		key = int(match.group('key'))
		id=match.group('actionId')
		if id[0] == '_':
			customActions[section].append((key, id, modifiers))
		else:
			numericActions[section].append((key, int(id), modifiers))
	return (numericActions, customActions)

def formatArray(scList, isCustom):
	s = ""
	for sc in scList:
		if s:
			s += ',\n\t'
		if isCustom:
			s += f'{{{sc[0]}, "{sc[1]}", {sc[2]}}}'
		else:
			s += f'{{{sc[0]}, {sc[1]}, {sc[2]}}}'
	return s

a,b = buildKeyList('../config/windows/reaper-kb.ini')
