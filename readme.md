## ----- ArghRad 3.00T9 by Tim Wright (Argh!) -----
## ----- TESTING VERSION, DO NOT REDISTRIBUTE -----
## Modified from original source code by id Software

This is a project to recover compilable C++ source code for the arghrad Quake 2 lighting utility (arghrad300t9.exe).

The decompiled source code has been merged with the original Quake 2 tools source so it should be fairly readable.

The main tools used were Ghidra, Ida and windbg. The process of creating the initial release took about 4 weeks using my spare time.



## Current status / TODO:

- Lighting is working, including bounce and phong.
- Supports JPG, TGA, PCX and WAL. Not supported yet: M32 and M8 formats.
- The -update flag is not implemented yet.
- Performance test.
- Test sunlight, all command args and entity options.

## Possible bugs to look for:

- Almost every line was ported by hand using the decompiler output as a reference point, so there is a good chance of finding a mistake anywhere.
- The ghidra decompiler likes to omit small patches of code, especially FPU code, so it is likely that some code is missing and needs to be found.
- Most conditional expressions using the FPU were decompiled by hand so I probably got some wrong.
- Larger functions are more likely to have bugs because they usually had lower quality decompilation results.
