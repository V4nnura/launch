# Accessories

<- [Contents](readme.md)


Help information for the Accessories is under development.
Please head online for the latest documentation. 
[https://launchfordos.app](https://launchfordos.app)

- Calendar
- Calculator
- Units
- Card Stack
- DOS Fetch
- Journal
- Markdown
- Note
- System File Editor
- Pixel Draw
- To-dos
- Typo
## System File Editor

`!SYSEDIT` transparently starts the System File Editor mode provided by `!NOTE /SYSEDIT`; there is only one full editor implementation. It edits the machine's principal configuration files. The DOS tab opens the boot drive's AUTOEXEC.BAT and CONFIG.SYS (FDAUTO.BAT and FDCONFIG.SYS on FreeDOS). When configured, Windows 3.x adds WIN.INI, SYSTEM.INI and PROGMAN.INI from the Launch! Windows path. When COMSPEC is 4DOS.COM, a 4DOS tab adds 4START.BAT and 4DOS.INI from that directory. Tabs and pages are fixed; saving writes the selected file back to its original location.
