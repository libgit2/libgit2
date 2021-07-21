#!/usr/bin/env ruby -W

file = ARGV[0]

raise "Missing argument" if file.nil?
raise "File not found" unless File.exist?(file)

opts = %w(
	-Xclang
	-ast-dump
	-Wall
	-fsyntax-only
	-fno-color-diagnostics
	-fparse-all-comments
	-iquote include
)
system("clang #{opts.join " "} #{file}")