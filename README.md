# hexdiff

## Description

This program reads data from one or more files and finds bytes or groups of bytes which are different. The differences are displayed in both hexadecimal and ASCII, and highlighted for easy comparison.

This implementation of hexdiff differs from similar programs by providing the ability to display calculated differences between files, ignore specific differences, change the length of highlighted differences, and seek or shift individual files to match up similar sections within different files. Other features include providing context around differing lines, changing the width of each file to display, and options to exclude specific areas of the output.

All of these features make this implementation an extremely powerful comparison and analysis tool.

## Examples

![Example 01](../extras/hexdiff-example-01.png?raw=true)
