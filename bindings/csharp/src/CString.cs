using System.Runtime.InteropServices;

namespace DazzleUpdater;

internal static class CString
{
    /// <summary>
    /// Takes ownership of a string returned by the C ABI and frees it.
    /// NULL (the C ABI's "no value") becomes <c>null</c>.
    /// </summary>
    internal static string? Take(nint ptr)
    {
        if (ptr == 0)
        {
            return null;
        }
        try
        {
            return Marshal.PtrToStringUTF8(ptr);
        }
        finally
        {
            NativeMethods.dazzle_free_c_str(ptr);
        }
    }

    /// <summary>
    /// C strings end at the first NUL, so anything after an interior NUL is dropped.
    /// </summary>
    internal static string? TruncateAtNul(string? value)
    {
        if (value is null)
        {
            return null;
        }
        int end = value.IndexOf('\0');
        return end < 0 ? value : value[..end];
    }
}
