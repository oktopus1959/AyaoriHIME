using Utils;

namespace KanchokuWS.Handler
{
    class TextOutputRouter
    {
        private static Logger logger = Logger.GetLogger();

        public static TextOutputRouter Singleton { get; private set; }

        public static TextOutputRouter CreateSingleton()
        {
            Singleton = new TextOutputRouter();
            return Singleton;
        }

        public static void DisposeSingleton()
        {
            Singleton?.Dispose();
            Singleton = null;
        }

        private readonly SendInputTextOutputSink sendInputSink = new SendInputTextOutputSink();
        private readonly TsfTextOutputSink tsfSink;
        private CompositionRoute compositionRoute;
        private bool precedingContextCached;
        private string precedingContext = "";

        private enum CompositionRoute
        {
            None,
            Tsf,
            SendInputLive,
            SendInputBuffered,
            TsfFaulted,
        }

        private TextOutputRouter()
        {
            tsfSink = new TsfTextOutputSink();
            tsfSink.CompositionTerminated += id => {
                compositionRoute = CompositionRoute.None;
                ResetPrecedingContext();
                CompositionTerminated?.Invoke(id);
            };
            tsfSink.ActiveClientChanged += ResetPrecedingContext;
        }

        public event System.Action<ulong> CompositionTerminated;

        public bool IsTsfCompositionActive =>
            compositionRoute == CompositionRoute.Tsf || compositionRoute == CompositionRoute.TsfFaulted;

        public void PrepareCompositionTarget()
        {
            if (Settings.UseTsfOutput) tsfSink.PrepareCompositionTarget();
        }

        public string GetPrecedingContext(bool allowFetch)
        {
            if (precedingContextCached) return precedingContext;
            if (!allowFetch) return "";
            precedingContextCached = true;
            precedingContext = "";
            if (!Settings.UseTsfOutput || IsTsfFallbackWindow()) return precedingContext;
            if (!tsfSink.TryGetPrecedingContext(16, out precedingContext, out string reason)) {
                precedingContext = "";
                logger.InfoH(() => $"TSF preceding context unavailable: {reason}");
            }
            return precedingContext;
        }

        private void ResetPrecedingContext()
        {
            precedingContextCached = false;
            precedingContext = "";
        }

        public void InvalidatePrecedingContext()
        {
            ResetPrecedingContext();
        }

        public void SendString(char[] str, int strLen, int numBS)
        {
            string reason = "";
            if (ShouldUseTsf(false) && tsfSink.TrySendString(str, strLen, numBS, out reason)) return;
            if (Settings.UseTsfOutput && reason._notEmpty()) logger.WarnH(() => $"TSF output fallback: {reason}");
            sendInputSink.TrySendString(str, strLen, numBS, out _);
        }

        public void SendStringViaClipboardIfNeeded(char[] str, int numBS, bool forceString = false)
        {
            string reason = "";
            if (ShouldUseTsf(forceString) && tsfSink.TrySendStringViaClipboardIfNeeded(str, numBS, forceString, out reason)) return;
            if (Settings.UseTsfOutput && reason._notEmpty()) logger.WarnH(() => $"TSF output fallback: {reason}");
            sendInputSink.TrySendStringViaClipboardIfNeeded(str, numBS, forceString, out _);
        }

        public void SendVKeyCombo(uint modifier, uint vkey, int count)
        {
            sendInputSink.SendVKeyCombo(modifier, vkey, count);
        }

        public bool UpdateComposition(string text, int caretOffset)
        {
            if (!Settings.UseTsfOutput) return false;
            if (compositionRoute == CompositionRoute.SendInputLive || compositionRoute == CompositionRoute.SendInputBuffered) return false;
            if (compositionRoute == CompositionRoute.TsfFaulted) return true;

            if (compositionRoute == CompositionRoute.None && IsTsfFallbackWindow()) {
                compositionRoute = Settings.UseEditWindow ? CompositionRoute.SendInputBuffered : CompositionRoute.SendInputLive;
                logger.InfoH(() => $"TSF composition disabled for window class: {ActiveWindowHandler.Singleton.ActiveWinClassName}");
                return false;
            }

            string reason;
            if (tsfSink.TryUpdateComposition(text, caretOffset, out reason)) {
                compositionRoute = CompositionRoute.Tsf;
                return true;
            }

            logger.WarnH(() => $"TSF composition fallback before start: {reason}");
            if (compositionRoute == CompositionRoute.Tsf || tsfSink.HasComposition) {
                compositionRoute = CompositionRoute.TsfFaulted;
                return true;
            }
            tsfSink.ResetComposition();
            compositionRoute = Settings.UseEditWindow ? CompositionRoute.SendInputBuffered : CompositionRoute.SendInputLive;
            return false;
        }

        public bool CommitComposition(int commitLength, bool hasRemainingText)
        {
            if (compositionRoute == CompositionRoute.Tsf || compositionRoute == CompositionRoute.TsfFaulted) {
                string reason = "";
                bool result = compositionRoute == CompositionRoute.Tsf && tsfSink.TryCommitComposition(commitLength, out reason);
                if (!result && reason._notEmpty()) logger.WarnH(() => $"TSF composition commit failed: {reason}");
                if (!hasRemainingText) {
                    tsfSink.ResetComposition();
                    compositionRoute = CompositionRoute.None;
                    ResetPrecedingContext();
                } else if (!result) {
                    compositionRoute = CompositionRoute.TsfFaulted;
                }
                return true;
            }

            bool outputAlreadySent = compositionRoute == CompositionRoute.SendInputLive;
            if (!hasRemainingText) compositionRoute = CompositionRoute.None;
            if (!hasRemainingText) ResetPrecedingContext();
            return outputAlreadySent;
        }

        public bool CancelComposition()
        {
            if (compositionRoute == CompositionRoute.Tsf || compositionRoute == CompositionRoute.TsfFaulted) {
                string reason;
                if (compositionRoute == CompositionRoute.Tsf && !tsfSink.TryCancelComposition(out reason) && reason._notEmpty()) {
                    logger.WarnH(() => $"TSF composition cancel failed: {reason}");
                }
                tsfSink.ResetComposition();
                compositionRoute = CompositionRoute.None;
                ResetPrecedingContext();
                return true;
            }
            compositionRoute = CompositionRoute.None;
            ResetPrecedingContext();
            return false;
        }

        private static bool ShouldUseTsf(bool forceString)
        {
            return Settings.UseTsfOutput && !forceString && !IsTsfFallbackWindow();
        }

        private static bool IsTsfFallbackWindow()
        {
            return Settings.IsTsfFallbackWinClass(ActiveWindowHandler.Singleton.ActiveWinClassName);
        }

        private void Dispose()
        {
            tsfSink.Dispose();
            sendInputSink.Dispose();
        }
    }
}
