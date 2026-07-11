namespace KanchokuWS.Handler
{
    class SendInputTextOutputSink : ITextOutputSink
    {
        public bool TrySendString(char[] str, int strLen, int numBS, out string failureReason)
        {
            failureReason = "";
            SendInputHandler.Singleton.SendString(str, strLen, numBS);
            return true;
        }

        public bool TrySendStringViaClipboardIfNeeded(char[] str, int numBS, bool forceString, out string failureReason)
        {
            failureReason = "";
            SendInputHandler.Singleton.SendStringViaClipboardIfNeeded(str, numBS, forceString);
            return true;
        }

        public void SendVKeyCombo(uint modifier, uint vkey, int count)
        {
            SendInputHandler.Singleton.SendVKeyCombo(modifier, vkey, count);
        }

        public void Dispose()
        {
        }
    }
}
