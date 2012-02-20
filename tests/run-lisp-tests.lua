-- run-lisp-tests
--
-- Copyright (c) 2010-2012 Free Software Foundation, Inc.
--
-- This file is part of GNU Zi.
--
-- This program is free software; you can redistribute it and/or modify it
-- under the terms of the GNU General Public License as published by
-- the Free Software Foundation; either version 3, or (at your option)
-- any later version.
--
-- This program is distributed in the hope that it will be useful, but
-- WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
-- General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program.  If not, see <http://www.gnu.org/licenses/>.

posix = require "posix"
require "std"

-- N.B. Tests that use execute-kbd-macro must note that keyboard input
-- is only evaluated once the script has finished running.

-- The following are defined in the environment for a build
local srcdir = os.getenv ("srcdir") or "."
local abs_srcdir = os.getenv ("abs_srcdir") or "."
local builddir = os.getenv ("builddir") or "."

local zi_pass = 0
local zi_fail = 0

local zi_cmd = io.catfile (builddir, "src", "zi")
local srcdir_pat = string.escapePattern (srcdir)

function run_test (test, name, editor_name, edit_file, cmd, args)
  posix.system ("cp", io.catfile (srcdir, "tests", "test.input"), edit_file)
  posix.system ("chmod", "+w", edit_file)
  local status = posix.system (cmd, unpack (args))
  if status == 0 then
    if posix.system ("diff", test .. ".output", edit_file) == 0 then
      posix.system ("rm", "-f", edit_file, edit_file .. "~")
      return true
    else
      print (editor_name .. " " .. name .. " failed to produce correct output")
    end
  else
    print (editor_name .. " " .. name .. " failed to run with error code " .. tostring (status))
  end
end

for _, name in ipairs (arg) do
  local test = name:gsub ("%.el$", "")
  if io.open (test .. ".output") ~= nil then
    name = test:gsub (io.catfile (srcdir, "tests/"), "")
    local edit_file = test:gsub ("^" .. srcdir_pat, builddir) .. ".input"
    local args = {"--no-init-file", edit_file, "--load", test:gsub ("^" .. srcdir_pat, abs_srcdir) .. ".el"}

    posix.system ("mkdir", "-p", posix.dirname (edit_file))

    if run_test (test, name, "Zi", edit_file, zi_cmd, args) then
      zi_pass = zi_pass + 1
    else
      zi_fail = zi_fail + 1
    end
  end
end

print (string.format ("Zi: %d pass(es) and %d failure(s)", zi_pass, zi_fail))

os.exit (zi_fail)
