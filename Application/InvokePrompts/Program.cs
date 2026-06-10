using System.Diagnostics;

var exeDir = AppContext.BaseDirectory;
var target = Path.Combine(exeDir, "Prompts", "Prompts.exe");

Process.Start(new ProcessStartInfo
{
    FileName = target,
    UseShellExecute = true,
    WorkingDirectory = Path.GetDirectoryName(target)
});
