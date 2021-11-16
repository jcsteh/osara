# OSARA: Open Source Accessibility for the REAPER Application
# Utility to build translation (pot) template
# Author: James Teh <jamie@jantrid.net>
# Copyright 2021 James Teh
# License: GNU General Public License version 2.0

import re
from collections import OrderedDict
import itertools

# Maps (context, msgid) to a dict of message data. We need this so we output
# only one entry for each message.
messages = OrderedDict()

def makePot(target, source, env):
	global messages
	out = open(target[0].path, "wt", encoding="UTF-8")
	out.write(
r"""msgid ""
msgstr ""
"Content-Type: text/plain; charset=UTF-8\n"
"Content-Transfer-Encoding: 8bit\n"

"""
	)
	for s in source:
		s = s.path
		inp = open(s, "rt", encoding="UTF-8")
		if s.endswith(".cpp"):
			addCpp(inp)
		elif s.endswith(".rc"):
			addRc(inp)
	for (context, msgid), data in messages.items():
		for comment in data.get("comments", ()):
			out.write('#. %s\n' % comment)
		if context:
			out.write('msgctxt "%s"\n' % context)
		out.write('msgid "%s"\n' % data["msgid"])
		if "plural" in data:
			out.write('msgid_plural "%s"\n' % data["plural"])
			out.write('msgstr[0] ""\n')
			out.write('msgstr[1] ""\n')
		else:
			out.write('msgstr ""\n')
		out.write("\n")

RE_TRANSLATORS_COMMENT = re.compile(r"^\s*// Translators: (.*)$")
RE_COMMENT = re.compile(r"^\s*// (.*)$")
inTranslatorsComment = False
lastTranslatorsComment = []
def handleTranslatorsComment(line):
	global inTranslatorsComment, lastTranslatorsComment
	m = RE_TRANSLATORS_COMMENT.match(line)
	if m:
		inTranslatorsComment = True
		lastTranslatorsComment.append(m.group(1))
		return True
	if inTranslatorsComment:
		m = RE_COMMENT.match(line)
		if m:
			lastTranslatorsComment.append(m.group(1))
			return True
		else:
			inTranslatorsComment = False
	return False

def addMessage(data):
	global messages, lastTranslatorsComment
	key = (data.get("context"), data["msgid"])
	data = messages.setdefault(key, data)
	if lastTranslatorsComment:
		comments = data.setdefault("comments", [])
		comments.extend(lastTranslatorsComment)
		lastTranslatorsComment = []

RE_CPP_TRANSLATE_FIRST_STRING_END = re.compile(r"^\s*// translate firstString end$")
RE_CPP_TRANSLATE_FIRST_STRING = re.compile(r'^\s*[^/].*?"(?P<msgid>.*?)"')
def addCppTranslateFirstString(input):
	for line in input:
		if handleTranslatorsComment(line):
			continue
		if RE_CPP_TRANSLATE_FIRST_STRING_END.match(line):
			break
		m = RE_CPP_TRANSLATE_FIRST_STRING.match(line)
		if m:
			data = m.groupdict()
			addMessage(data)

RE_CPP_TRANSLATE = re.compile(r'\btranslate\("(?P<msgid>.*?)"\)')
RE_CPP_TRANSLATE_CTXT = re.compile(r'\btranslate_ctxt\("(?P<context>.*?)",\s*"(?P<msgid>.*?)\)')
RE_CPP_TRANSLATE_PLURAL = re.compile(r'\btranslate_plural\("(?P<msgid>.*?)",\s*"(?P<plural>.*?)", .*?\)')
RE_CPP_TRANSLATE_FIRST_STRING_BEGIN = re.compile(r"^\s*// translate firstString begin$")
def addCpp(input):
	for line in input:
		if handleTranslatorsComment(line):
			continue
		if RE_CPP_TRANSLATE_FIRST_STRING_BEGIN.match(line):
			addCppTranslateFirstString(input)
			continue
		matches = itertools.chain(RE_CPP_TRANSLATE.finditer(line),
			RE_CPP_TRANSLATE_CTXT.finditer(line),
			RE_CPP_TRANSLATE_PLURAL.finditer(line))
		for m in matches:
			addMessage(m.groupdict())

RE_RC_TRANSLATE = re.compile(r'^\s*(?P<command>CAPTION|LTEXT|DEFPUSHBUTTON|PUSHBUTTON|GROUPBOX|CONTROL)\s+"(?P<msgid>.*?)"')
def addRc(input):
	context = None
	for line in input:
		if handleTranslatorsComment(line):
			continue
		m = RE_RC_TRANSLATE.match(line)
		if m:
			data = m.groupdict()
			if data["command"] == "CAPTION":
				context = data["msgid"]
			if not context:
				raise RuntimeError("No caption before messages")
			if not data["msgid"]:
				continue
			data["context"] = context
			addMessage(data)
