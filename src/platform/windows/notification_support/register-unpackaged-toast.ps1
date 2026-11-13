# Registra la identidad de notificaciones de una compilación sin empaquetar.
#
# ToastNotificationManager exige que el AppUserModelID usado al crear el
# notificador esté respaldado por un acceso directo del menú Inicio. Este script
# crea ese acceso y declara el nombre visible en el registro del usuario.
#
# Uso (PowerShell de usuario, sin elevación):
#   powershell -ExecutionPolicy Bypass -File register-unpackaged-toast.ps1 `
#       -TargetPath "C:\ruta\OneInTwo.exe"
#
# Verificación:
#   - El acceso aparece en %APPDATA%\Microsoft\Windows\Start Menu\Programs.
#   - La notificación de presentación del recorrido nativo se muestra.
#   - Si Windows no la muestra, revisar que el AUMID del acceso coincida
#     exactamente con "OneInTwo.Desktop".

param(
    [Parameter(Mandatory = $true)]
    [string]$TargetPath,
    [string]$Aumid = "OneInTwo.Desktop",
    [string]$DisplayName = "OneInTwo",
    [string]$ShortcutName = "OneInTwo.lnk"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $TargetPath -PathType Leaf)) {
    throw "No existe el ejecutable indicado: $TargetPath"
}
$TargetPath = (Resolve-Path -LiteralPath $TargetPath).Path

if (-not ("OneInTwoToastInterop.ShortcutWriter" -as [type])) {
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

namespace OneInTwoToastInterop
{
    [ComImport, Guid("00021401-0000-0000-C000-000000000046")]
    internal class ShellLinkCoClass { }

    [ComImport, InterfaceType(ComInterfaceType.InterfaceIsIUnknown),
     Guid("000214F9-0000-0000-C000-000000000046")]
    internal interface IShellLinkW
    {
        void GetPath([Out, MarshalAs(UnmanagedType.LPWStr)] System.Text.StringBuilder pszFile,
                     int cch, IntPtr pfd, uint fFlags);
        void GetIDList(out IntPtr ppidl);
        void SetIDList(IntPtr pidl);
        void GetDescription([Out, MarshalAs(UnmanagedType.LPWStr)] System.Text.StringBuilder pszName,
                            int cch);
        void SetDescription([MarshalAs(UnmanagedType.LPWStr)] string pszName);
        void GetWorkingDirectory([Out, MarshalAs(UnmanagedType.LPWStr)] System.Text.StringBuilder pszDir,
                                 int cch);
        void SetWorkingDirectory([MarshalAs(UnmanagedType.LPWStr)] string pszDir);
        void GetArguments([Out, MarshalAs(UnmanagedType.LPWStr)] System.Text.StringBuilder pszArgs,
                          int cch);
        void SetArguments([MarshalAs(UnmanagedType.LPWStr)] string pszArgs);
        void GetHotkey(out short pwHotkey);
        void SetHotkey(short wHotkey);
        void GetShowCmd(out int piShowCmd);
        void SetShowCmd(int iShowCmd);
        void GetIconLocation([Out, MarshalAs(UnmanagedType.LPWStr)] System.Text.StringBuilder pszIconPath,
                             int cch, out int piIcon);
        void SetIconLocation([MarshalAs(UnmanagedType.LPWStr)] string pszIconPath, int iIcon);
        void SetRelativePath([MarshalAs(UnmanagedType.LPWStr)] string pszPathRel, uint dwReserved);
        void Resolve(IntPtr hwnd, uint fFlags);
        void SetPath([MarshalAs(UnmanagedType.LPWStr)] string pszFile);
    }

    [ComImport, InterfaceType(ComInterfaceType.InterfaceIsIUnknown),
     Guid("0000010B-0000-0000-C000-000000000046")]
    internal interface IPersistFile
    {
        void GetClassID(out Guid pClassID);
        [PreserveSig] int IsDirty();
        void Load([MarshalAs(UnmanagedType.LPWStr)] string pszFileName, uint dwMode);
        void Save([MarshalAs(UnmanagedType.LPWStr)] string pszFileName,
                  [MarshalAs(UnmanagedType.Bool)] bool fRemember);
        void SaveCompleted([MarshalAs(UnmanagedType.LPWStr)] string pszFileName);
        void GetCurFile([MarshalAs(UnmanagedType.LPWStr)] out string ppszFileName);
    }

    [ComImport, InterfaceType(ComInterfaceType.InterfaceIsIUnknown),
     Guid("886D8EEB-8CF2-4446-8D02-CDBA1DBDCF99")]
    internal interface IPropertyStore
    {
        void GetCount(out uint cProps);
        void GetAt(uint iProp, out PropertyKey pkey);
        void GetValue(ref PropertyKey key, out PropVariant pv);
        void SetValue(ref PropertyKey key, ref PropVariant pv);
        void Commit();
    }

    [StructLayout(LayoutKind.Sequential, Pack = 4)]
    internal struct PropertyKey
    {
        public Guid fmtid;
        public uint pid;
    }

    [StructLayout(LayoutKind.Explicit)]
    internal struct PropVariant
    {
        [FieldOffset(0)] public ushort vt;
        [FieldOffset(8)] public IntPtr pointerValue;
    }

    public static class ShortcutWriter
    {
        private static readonly PropertyKey AppUserModelId = new PropertyKey
        {
            fmtid = new Guid("9F4C2855-9F79-4B39-A8D0-E1D42DE1D5F3"),
            pid = 5
        };

        public static void Create(string shortcutPath, string targetPath, string aumid)
        {
            var link = (IShellLinkW)new ShellLinkCoClass();
            link.SetPath(targetPath);
            link.SetWorkingDirectory(System.IO.Path.GetDirectoryName(targetPath));

            var store = (IPropertyStore)link;
            var value = new PropVariant
            {
                vt = 31, // VT_LPWSTR
                pointerValue = Marshal.StringToCoTaskMemUni(aumid)
            };
            try
            {
                store.SetValue(ref AppUserModelId, ref value);
                store.Commit();
                ((IPersistFile)link).Save(shortcutPath, true);
            }
            finally
            {
                Marshal.FreeCoTaskMem(value.pointerValue);
            }
        }
    }
}
"@
}

$startMenu = [Environment]::GetFolderPath("Programs")
$shortcutPath = Join-Path $startMenu $ShortcutName
[OneInTwoToastInterop.ShortcutWriter]::Create($shortcutPath, $TargetPath, $Aumid)

$registryPath = "HKCU:\Software\Classes\AppUserModelId\$Aumid"
New-Item -Path $registryPath -Force | Out-Null
Set-ItemProperty -Path $registryPath -Name DisplayName -Value $DisplayName
Set-ItemProperty -Path $registryPath -Name IconUri -Value $TargetPath

Write-Host "Acceso directo: $shortcutPath"
Write-Host "AppUserModelID: $Aumid"
Write-Host "Registro: $registryPath"
