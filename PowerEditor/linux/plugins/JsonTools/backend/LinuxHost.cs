// SPDX-License-Identifier: Apache-2.0
// The upstream parser expects the WinForms front end to translate lint messages.

namespace JSON_Tools.Utils
{
    internal static class Translator
    {
        internal static string TranslateLintMessage<T>(bool translated, T key, string english)
        {
            return english;
        }
    }
}
