' Runs a batch file with no console window, so that nobody sees one: the desktop shortcut and "Play SSBU.bat" itself use it.
' Usage: wscript //nologo tools\run_hidden.vbs "<batch file>" [its arguments]
' The batch file is told it is hidden (NATIVESMASH_HIDDEN=1), so that it does not start this again.
Dim shell, fso, bat, args, i
Set shell = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")
If WScript.Arguments.Count = 0 Then WScript.Quit 1
bat = fso.GetAbsolutePathName(WScript.Arguments(0))
args = ""
For i = 1 To WScript.Arguments.Count - 1
    args = args & " " & WScript.Arguments(i)
Next
shell.Environment("Process")("NATIVESMASH_HIDDEN") = "1"
shell.CurrentDirectory = fso.GetParentFolderName(bat)
shell.Run "cmd.exe /c " & Chr(34) & Chr(34) & bat & Chr(34) & args & Chr(34), 0, False
