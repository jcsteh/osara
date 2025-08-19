# OSARA: Open Source Accessibility for the REAPER Application
# Utility to build translation (pot) template
# Copyright 2021-2023 James Teh
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
		if s.endswith(".cpp") or s.endswith(".mm"):
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
	if not data["msgid"]:
		raise RuntimeError("Empty msgid")
	key = (data.get("context"), data["msgid"])
	data = messages.setdefault(key, data)
	if lastTranslatorsComment:
		comments = data.setdefault("comments", [])
		comments.extend(lastTranslatorsComment)
		lastTranslatorsComment = []

RE_CPP_TRANSLATE = re.compile(r'\b(?:translate|_t)\(\s*(?:"(?P<msgid>.*?)"|[^)]*)\s*(?P<end>\))?')
RE_CPP_TRANSLATE_CTXT = re.compile(r'\btranslate_ctxt\(\s*(?:"(?P<context>.*?)"|[^)]*),?\s*(?:"(?P<msgid>.*?)"|[^)]*)\s*(?P<end>\))?')
RE_CPP_TRANSLATE_PLURAL = re.compile(r'\btranslate_plural\(\s*(?:"(?P<msgid>.*?)"|[^)]*),?\s*(?:"(?P<plural>.*?)"|[^)]*),?\s*[^)]*\s*(?P<end>\))?')
def addCpp(input):
	for line in input:
		if handleTranslatorsComment(line):
			continue
		while True:
			matches = list(RE_CPP_TRANSLATE.finditer(line))
			matches.extend(RE_CPP_TRANSLATE_CTXT.finditer(line))
			matches.extend(RE_CPP_TRANSLATE_PLURAL.finditer(line))
			# These regexps match even if the call is incomplete. For a complete call,
			# the "end" match group will be ")". For an incomplete call, the "end"
			# match group will be None.
			if all(m.group("end") for m in matches):
				# All translate calls are complete.
				break
			# There is an incomplete translate call. It must continue onto the next
			# line. Add the next line and try again.
			line += next(input)
		for m in matches:
			if not m.group("msgid"):
				# This can happen if this is a runtime translation where the msgid is a
				# variable.
				continue
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
