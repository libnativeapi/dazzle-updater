using System;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Dazzle;

/// <summary>
/// Finds the C ABI library under the name this binding deploys it with.
/// </summary>
/// <remarks>
/// <para>
/// Windows filesystems are case-insensitive, so the native <c>dazzle.dll</c> and this
/// managed assembly (<c>Dazzle.dll</c>) cannot sit beside each other: whichever is copied
/// last replaces the other, and the assembly fails to load. The package therefore deploys
/// the native library as <c>dazzle_native.dll</c> there and this resolver answers the
/// <c>dazzle</c> import with it. Elsewhere the names already differ (<c>libdazzle.so</c>,
/// <c>libdazzle.dylib</c>) and the default probing is left alone.
/// </para>
/// </remarks>
internal static class NativeLibraryResolver
{
    // CA2255 warns libraries off module initializers. This one is the point: the resolver
    // has to be registered before the first P/Invoke, whenever that happens, and a module
    // initializer is the only place that guarantees it without the consumer doing anything.
#pragma warning disable CA2255
    [ModuleInitializer]
    internal static void Initialize()
    {
        if (OperatingSystem.IsWindows())
        {
            NativeLibrary.SetDllImportResolver(
                typeof(NativeLibraryResolver).Assembly, ResolveWindows);
        }
    }
#pragma warning restore CA2255

    private static IntPtr ResolveWindows(
        string libraryName, Assembly assembly, DllImportSearchPath? searchPath)
    {
        return libraryName == "dazzle"
            ? NativeLibrary.Load("dazzle_native.dll", assembly, searchPath)
            : IntPtr.Zero;  // anything else: the runtime's own probing
    }
}
