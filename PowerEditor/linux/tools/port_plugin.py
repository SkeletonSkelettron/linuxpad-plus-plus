#!/usr/bin/env python3
# This file is part of Notepad++ project
# Copyright (C)2026 Notepad++ Linux port contributors
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# at your option any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

"""Copies the sources of a Windows plugin into plugins/<Name>/ for the Linux plugin SDK.

port_plugin.py <repository> <Name> [--root <folder of the repository>] [--exclude <glob>]...
               [--url <url>] [--license <name>]

- C/C++ sources, headers, .rc files, the files they use (icons, bitmaps...) and the license
  files are copied, keeping their folders.
- The copies of the Notepad++ and Scintilla headers that plugins carry are left out: the SDK
  has them (plugin-sdk/include).
- '\\' in the paths of #include lines and of .rc files becomes '/'.
- CMakeLists.txt calls npp_add_plugin() with the sources and the .rc files; edit it afterwards
  when the plugin needs more (definitions, data files).
"""

import argparse
import fnmatch
import os
import re
import shutil
import subprocess
import sys

SDK_HEADERS = {
	"plugininterface.h", "notepad_plus_msgs.h", "scintilla.h", "scilexer.h", "sci_position.h",
	"menucmdid.h", "docking.h", "dockingresource.h", "ilexer.h", "lexilla.h", "scintillamessages.h",
	"scintillatypes.h", "scintillastructures.h", "scintillacall.h", "boostregexsearch.h",
}
SOURCE_EXT = {".c", ".cpp", ".cxx", ".cc"}
HEADER_EXT = {".h", ".hh", ".hpp", ".hxx", ".inl", ".inc", ".rh"}
RC_EXT = {".rc", ".rc2"}
DATA_EXT = {".ico", ".bmp", ".png", ".cur", ".manifest", ".xpm", ".gif", ".jpg"}
SKIP_DIRS = {".git", ".github", ".vs", "x64", "Win32", "Debug", "Release", "bin", "obj", "packages", "test", "tests", "UnitTest", "unittest"}

INCLUDE_RE = re.compile(r'^(\s*#\s*include\s*[<"])([^>"]+)([>"])', re.M)
RC_PATH_RE = re.compile(r'"([^"\n]*\\\\[^"\n]*)"')


def read(path):
	with open(path, "rb") as f:
		return f.read()


def fix_includes(text):
	def include(m):
		path = m.group(2).replace("\\", "/")
		# a copy of a Notepad++/Scintilla header in another folder: the one of the SDK
		if "/" in path and os.path.basename(path).lower() in SDK_HEADERS:
			return m.group(1)[:-1] + "<" + os.path.basename(path) + ">"
		return m.group(1) + path + m.group(3)
	return INCLUDE_RE.sub(include, text)


def fix_rc_paths(text):
	# ICON "res\\x.ico", BITMAP "res\\x.bmp" (string paths of files that exist next to the .rc)
	def path(m):
		p = m.group(1)
		if re.search(r'\.(ico|bmp|png|cur|manifest|xpm|gif|jpg|rc2|h)$', p, re.I):
			return '"' + p.replace("\\\\", "/") + '"'
		return m.group(0)
	return RC_PATH_RE.sub(path, text)


def fix_include_case(dest, include_dirs):
	"""#include "process.h" of a file Process.h: Windows ignores the case, Linux does not."""
	folders = [""] + [d for d in include_dirs if d]
	names = {}
	for folder in folders:
		path = os.path.join(dest, folder)
		if os.path.isdir(path):
			for f in os.listdir(path):
				names.setdefault((folder, f.lower()), f)
	for root, _, files in os.walk(dest):
		for f in files:
			if os.path.splitext(f)[1].lower() not in SOURCE_EXT | HEADER_EXT | RC_EXT:
				continue
			p = os.path.join(root, f)
			text, enc = decode(read(p))
			here = os.path.relpath(root, dest).replace(os.sep, "/")
			here = "" if here == "." else here

			def include(m):
				if m.group(1).rstrip().endswith("<"):
					return m.group(0)
				inc = m.group(2)
				folder, base = os.path.split(inc)
				for d in [here] + folders:
					where = os.path.normpath(os.path.join(d, folder)).replace(os.sep, "/")
					where = "" if where == "." else where
					if os.path.exists(os.path.join(dest, where, base)):
						return m.group(0)
					real = names.get((where, base.lower()))
					if real is None and os.path.isdir(os.path.join(dest, where)):
						real = next((x for x in os.listdir(os.path.join(dest, where)) if x.lower() == base.lower()), None)
					if real:
						return m.group(1) + (folder + "/" if folder else "") + real + m.group(3)
				return m.group(0)

			fixed = INCLUDE_RE.sub(include, text)
			if fixed != text:
				with open(p, "wb") as o:
					o.write(fixed.encode("utf-8" if enc == "utf-8-sig" else enc))


def decode(data):
	if data.startswith(b"\xff\xfe") or data.startswith(b"\xfe\xff"):
		return data.decode("utf-16"), "utf-16"
	if data.startswith(b"\xef\xbb\xbf"):
		return data[3:].decode("utf-8"), "utf-8-sig"
	try:
		return data.decode("utf-8"), "utf-8"
	except UnicodeDecodeError:
		return data.decode("cp1252"), "cp1252"


def main():
	ap = argparse.ArgumentParser()
	ap.add_argument("repository")
	ap.add_argument("name")
	ap.add_argument("--root", default="", help="folder of the repository that has the plugin")
	ap.add_argument("--exclude", action="append", default=[], help="glob of paths to leave out")
	ap.add_argument("--url", default="")
	ap.add_argument("--license", default="", help="license declared by the upstream source")
	args = ap.parse_args()

	repo = os.path.abspath(args.repository)
	root = os.path.join(repo, args.root)
	here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
	dest = os.path.join(here, "plugins", args.name)
	if os.path.exists(dest):
		sys.exit(f"{dest} exists")

	sources, rcs, include_dirs = [], [], set()
	for folder, dirs, files in os.walk(root):
		dirs[:] = sorted(d for d in dirs if d not in SKIP_DIRS)
		for f in sorted(files):
			src = os.path.join(folder, f)
			rel = os.path.relpath(src, root).replace(os.sep, "/")
			if any(fnmatch.fnmatch(rel, g) for g in args.exclude):
				continue
			ext = os.path.splitext(f)[1].lower()
			base = f.lower()
			is_license = base.startswith(("license", "licence", "copying"))
			if ext in HEADER_EXT and base in SDK_HEADERS:
				continue
			if not (ext in SOURCE_EXT or ext in HEADER_EXT or ext in RC_EXT or ext in DATA_EXT or is_license):
				continue
			out = os.path.join(dest, rel)
			os.makedirs(os.path.dirname(out), exist_ok=True)
			if ext in SOURCE_EXT or ext in HEADER_EXT or ext in RC_EXT:
				data = read(src)
				text, enc = decode(data)
				fixed = fix_includes(text)
				if ext in RC_EXT:
					fixed = fix_rc_paths(fixed)
				if fixed != text:
					with open(out, "wb") as o:
						o.write(fixed.encode("utf-8" if enc == "utf-8-sig" else enc))
				else:
					shutil.copyfile(src, out)
			else:
				shutil.copyfile(src, out)
			if ext in SOURCE_EXT:
				sources.append(rel)
			elif ext == ".rc":
				rcs.append(rel)
			elif ext in HEADER_EXT:
				include_dirs.add(os.path.dirname(rel))

	fix_include_case(dest, sorted(include_dirs))
	rev = subprocess.run(["git", "-C", repo, "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip()
	dllmain = False
	for s in sources:
		text, _ = decode(read(os.path.join(dest, s)))
		if re.search(r"\bDllMain\s*\(", text):
			dllmain = True
	# .rc files #included by another .rc are compiled with it
	included = set()
	for r in rcs:
		text, _ = decode(read(os.path.join(dest, r)))
		for m in INCLUDE_RE.finditer(text):
			included.add(os.path.normpath(os.path.join(os.path.dirname(r), m.group(2))).replace(os.sep, "/"))
	rcs = [r for r in rcs if r not in included]

	license_note = f", {args.license}" if args.license else ""
	lines = [f"# {args.name} ({args.url or os.path.basename(repo)}){license_note}.",
		f"# Source revision {rev}.",
		f"npp_add_plugin({args.name}{' DLLMAIN' if dllmain else ''}"]
	lines.append("\t" + " ".join(sources))
	if rcs:
		lines.append("\tRC " + " ".join(rcs))
	lines[-1] += ")"
	dirs = sorted(d for d in include_dirs if d)
	inc = " ".join(['"${CMAKE_CURRENT_SOURCE_DIR}"'] + [f'"${{CMAKE_CURRENT_SOURCE_DIR}}/{d}"' for d in dirs])
	lines.append(f"target_include_directories({args.name} PRIVATE {inc})")
	with open(os.path.join(dest, "CMakeLists.txt"), "w") as f:
		f.write("\n".join(lines) + "\n")
	print(f"{args.name}: {len(sources)} sources, rc {rcs}, revision {rev}")


if __name__ == "__main__":
	main()
