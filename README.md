# hexdiff

## Description

This program reads data from one or more files and finds bytes or groups of bytes which are different. The differences are displayed in both hexadecimal and ASCII, and highlighted for easy comparison.

This implementation of hexdiff differs from similar programs by providing the ability to display calculated differences between files, ignore specific differences, change the length of highlighted differences, and seek or shift individual files to match up similar sections within different files. Other features include providing context around differing lines, changing the width of each file to display, and options to exclude specific areas of the output.

All of these features make this implementation an extremely powerful comparison and analysis tool.

## Examples

The following example sets the number of bytes to include on each line of each data set to 8 (-w 8), displays an additional data set which is the mathematical difference between the other data sets (-d), sets the number of difference bytes to highlight to 4 (-h 4) which also affects the calculated differences, and displays 1 line of context (-c 1) before and after any differences.

![Example 01](../extras/hexdiff-example-01.png?raw=true)

The following example shifts the second data set (file number 1) by 0x20 bytes (-S 1:0x20) which inserts 32 bytes of NULLs at the beginning so the files match up, displays all lines regardless of differences (-v), treats NULL bytes as different (-N), and sets the maximum length to 128 bytes (-l 128). Due to the inserted NULLs, the number of bytes processed on the second data set is only 96 bytes.

![Example 02](../extras/hexdiff-example-02.png?raw=true)
