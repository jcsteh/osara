import sortKeymap

def buildKeyList(fn):
	lines = open(fn, encoding='utf-8').readlines()
	numericCommands = {
		0:[], 100:[], 102:[], 103:[],
		32060:[], 32061:[], 32062:[], 32063:[]}
	namedCommands = {
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
		if id == 0:
			continue
		if id[0] == '_':
			namedCommands[section].append((key, id, modifiers))
		else:
			numericCommands[section].append((key, int(id), modifiers))
	return (numericCommands, namedCommands)

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

s = """
struct NamedKeyBindingInfo {
	int key;
	const char* cmd;
	int flags;
};

"""

a,b = buildKeyList('../config/windows/reaper-kb.ini')
sections = ''
for section in a:
	s += f'vector<KbdKeyBindingInfo> osaraKeySection{section}{{\n'
	s += formatArray(a[section], False)
	s+= '\n};\n'
	if sections:
		sections += ',\n'
	sections += f'\t{{{section}, osaraKeySection{section}, osaraNamedKeySection{section}}}'
for section in b:
	s += f'vector<NamedKeyBindingInfo> osaraNamedKeySection{section}{{\n'
	s += formatArray(b[section], True)
	s+= '\n};\n'

s += """
struct OsaraKeySection {
	int section;
	vector<KbdKeyBindingInfo>& keys;
	vector<NamedKeyBindingInfo>& namedKeys;
};
vector<OsaraKeySection> osaraKeySections {
"""
s += sections;
s += """
};
"""
	

print(s)