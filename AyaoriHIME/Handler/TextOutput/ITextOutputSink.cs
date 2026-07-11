using System;

namespace KanchokuWS.Handler
{
    interface ITextOutputSink : IDisposable
    {
        bool TrySendString(char[] str, int strLen, int numBS, out string failureReason);

        bool TrySendStringViaClipboardIfNeeded(char[] str, int numBS, bool forceString, out string failureReason);

        void SendVKeyCombo(uint modifier, uint vkey, int count);
    }
}
