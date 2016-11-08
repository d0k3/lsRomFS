# lsRomFS
Simple code for learning how RomFS level 3 works

Usage: Usage: lsrfs [romfs file] [path in romfs]

[romfs file] has to be decrypted. Use [ctrtool](https://github.com/profi200/Project_CTR) and [Decrypt9](https://github.com/d0k3/Decrypt9WIP) to extract & decrypt the RomFS from any CIA / 3DS / NCCH.

[path in romfs] uses '/' as separators. If it leads to a directory, directory is listed, if it leads to a file, the file is dumped.

See license.txt for license
