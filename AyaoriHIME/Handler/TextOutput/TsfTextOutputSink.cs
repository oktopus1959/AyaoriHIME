using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Pipes;
using System.Security.Principal;
using System.Text;
using System.Threading;
using Utils;

namespace KanchokuWS.Handler
{
    class TsfTextOutputSink : ITextOutputSink
    {
        private static readonly Logger logger = Logger.GetLogger();

        private const int Magic = 0x54465341; // ASFT
        private const short Version = 2;
        private const short MessageHello = 1;
        private const short MessageFocusChanged = 2;
        private const short MessageUpdateComposition = 3;
        private const short MessageCommitComposition = 4;
        private const short MessageCancelComposition = 5;
        private const short MessageOperationResult = 6;
        private const short MessageCompositionTerminated = 7;
        private const short MessageBye = 8;
        private const int HresultOk = 0;
        private const int HresultFail = unchecked((int)0x80004005);
        private const int MaxPayloadLength = 1024 * 1024;

        private readonly string pipeName;
        private readonly object sync = new object();
        private readonly List<ClientConnection> clients = new List<ClientConnection>();
        private NamedPipeServerStream listeningPipe;
        private ClientConnection activeClient;
        private bool disposed;
        private int nextClientId;
        private long nextCompositionId;
        private ulong compositionId;
        private ulong compositionSequence;
        private ClientConnection compositionClient;

        public event Action<ulong> CompositionTerminated;

        public TsfTextOutputSink()
        {
            pipeName = "AyaoriHIME.TsfOutput." + GetUserSidForPipeName();
            BeginAccept();
        }

        public bool TrySendString(char[] str, int strLen, int numBS, out string failureReason)
        {
            failureReason = "direct text output is not a TSF composition operation";
            return false;
        }

        public bool TrySendStringViaClipboardIfNeeded(char[] str, int numBS, bool forceString, out string failureReason)
        {
            if (forceString) {
                failureReason = "forceString output is not handled by TSF";
                return false;
            }
            failureReason = "direct text output is not a TSF composition operation";
            return false;
        }

        public bool HasComposition => compositionId != 0;

        public bool TryUpdateComposition(string text, int caretOffset, out string failureReason)
        {
            text = text ?? "";
            if (caretOffset < 0 || caretOffset > text.Length) {
                failureReason = "invalid TSF composition caret";
                return false;
            }
            ClientConnection client = GetActiveClient();
            if (client == null) {
                failureReason = "active TSF client is not connected";
                return false;
            }
            bool isNewComposition = compositionId == 0;
            if (isNewComposition) {
                compositionId = unchecked((ulong)Interlocked.Increment(ref nextCompositionId));
                if (compositionId == 0) compositionId = unchecked((ulong)Interlocked.Increment(ref nextCompositionId));
                compositionSequence = 0;
                compositionClient = client;
            } else if (!ReferenceEquals(compositionClient, client)) {
                failureReason = "active TSF client changed during composition";
                return false;
            }
            bool result = TryOperation(client, MessageUpdateComposition, ++compositionSequence, text, caretOffset, 0, out failureReason);
            if (!result && isNewComposition) ResetComposition();
            return result;
        }

        public bool TryCommitComposition(int commitLength, out string failureReason)
        {
            if (compositionId == 0) { failureReason = ""; return true; }
            bool ok = TryOperation(compositionClient, MessageCommitComposition, ++compositionSequence, null, 0, commitLength, out failureReason);
            if (ok && commitLength >= 0) {
                // 部分確定後も論理compositionは継続する。全文確定の判定はrouterがResetCompositionで行う。
            }
            return ok;
        }

        public bool TryCancelComposition(out string failureReason)
        {
            if (compositionId == 0) { failureReason = ""; return true; }
            bool ok = TryOperation(compositionClient, MessageCancelComposition, ++compositionSequence, null, 0, 0, out failureReason);
            if (ok) ResetComposition();
            return ok;
        }

        public void ResetComposition()
        {
            compositionId = 0;
            compositionSequence = 0;
            compositionClient = null;
        }

        private void OnCompositionTerminated(byte[] payload)
        {
            ulong terminatedId = payload != null && payload.Length >= 8 ? BitConverter.ToUInt64(payload, 0) : 0;
            if (terminatedId == 0 || terminatedId != compositionId) return;
            ResetComposition();
            CompositionTerminated?.Invoke(terminatedId);
        }

        private bool TryOperation(ClientConnection client, short operation, ulong sequence, string text, int caretOffset, int commitLength, out string failureReason)
        {
            if (client == null || !client.IsConnected) {
                failureReason = "active TSF client is disconnected";
                return false;
            }
            int hresult;
            string error;
            CommitOutcome outcome = client.TryOperation(operation, compositionId, sequence, text, caretOffset, commitLength,
                Settings.TsfOutputTimeoutMillisec, out hresult, out error);
            if (outcome == CommitOutcome.Success) { failureReason = ""; return true; }
            if (outcome == CommitOutcome.TimeoutAfterDispatch) {
                failureReason = error._notEmpty() ? error : "TSF composition timeout after dispatch";
                logger.WarnH(failureReason);
                // dispatch 後は TSF 側で編集済みの可能性があるため、接続を維持して
                // 遅延応答を reader に回収させる。
                return true;
            }
            failureReason = outcome == CommitOutcome.ExplicitFailure
                ? $"TSF composition failed: HRESULT=0x{unchecked((uint)hresult):X8}"
                : (error._notEmpty() ? error : "active TSF client is disconnected");
            return false;
        }

        public void SendVKeyCombo(uint modifier, uint vkey, int count)
        {
            SendInputHandler.Singleton.SendVKeyCombo(modifier, vkey, count);
        }

        public void Dispose()
        {
            List<ClientConnection> closingClients;
            NamedPipeServerStream closingListener;

            lock (sync) {
                disposed = true;
                closingListener = listeningPipe;
                listeningPipe = null;
                closingClients = new List<ClientConnection>(clients);
                clients.Clear();
                activeClient = null;
            }

            closingListener?.Dispose();
            foreach (var client in closingClients) {
                client.Dispose();
            }
        }

        private ClientConnection GetActiveClient()
        {
            lock (sync) {
                if (activeClient == null) {
                    logger.InfoH("TSF active client lookup: no focused client");
                    return null;
                }

                bool isConnected = activeClient.IsConnected;
                if (isConnected) return activeClient;
                logger.InfoH(() => $"TSF active client lookup: client={activeClient.Id}, connected={isConnected}, registered={clients.Contains(activeClient)}");
                activeClient = null;
                return null;
            }
        }

        private void BeginAccept()
        {
            NamedPipeServerStream pipe = null;
            try {
                lock (sync) {
                    if (disposed) return;
                }

                pipe = new NamedPipeServerStream(
                    pipeName,
                    PipeDirection.InOut,
                    16,
                    PipeTransmissionMode.Byte,
                    PipeOptions.Asynchronous);

                lock (sync) {
                    if (disposed) {
                        pipe.Dispose();
                        return;
                    }
                    listeningPipe = pipe;
                }

                pipe.BeginWaitForConnection(OnPipeConnected, pipe);
            } catch (Exception ex) {
                pipe?.Dispose();
                lock (sync) {
                    if (disposed) return;
                }
                logger.WarnH(() => $"TSF pipe listen failed: {ex.Message}");
            }
        }

        private void OnPipeConnected(IAsyncResult ar)
        {
            var pipe = (NamedPipeServerStream)ar.AsyncState;
            bool accepted = false;
            try {
                pipe.EndWaitForConnection(ar);
                accepted = true;
            } catch (ObjectDisposedException) {
                return;
            } catch (Exception ex) {
                lock (sync) {
                    if (!disposed) logger.WarnH(() => $"TSF pipe accept failed: {ex.Message}");
                }
            }

            if (accepted) {
                ClientConnection client = null;
                lock (sync) {
                    if (ReferenceEquals(listeningPipe, pipe)) listeningPipe = null;
                    if (!disposed) {
                        client = new ClientConnection(this, pipe, ++nextClientId);
                        clients.Add(client);
                    }
                }

                if (client != null) {
                    logger.InfoH(() => $"TSF pipe connected: {pipeName}, client={client.Id}");
                    client.Start();
                } else {
                    pipe.Dispose();
                    return;
                }
            } else {
                pipe.Dispose();
            }

            BeginAccept();
        }

        private void OnClientHello(ClientConnection client)
        {
            logger.InfoH(() => $"TSF client hello: client={client.Id}");
        }

        private void OnFocusChanged(ClientConnection client, bool hasFocus)
        {
            lock (sync) {
                if (disposed || !clients.Contains(client)) return;
                if (client.ProtocolVersion != Version) {
                    logger.InfoH(() => $"TSF client focus ignored: client={client.Id}, protocol={client.ProtocolVersion}");
                    return;
                }
                if (hasFocus) {
                    activeClient = client;
                } else if (ReferenceEquals(activeClient, client)) {
                    activeClient = null;
                }
            }
            logger.InfoH(() => $"TSF client focus changed: client={client.Id}, focus={hasFocus}");
        }

        private void OnClientBye(ClientConnection client)
        {
            DisconnectClient(client, "TSF client bye");
        }

        private void DisconnectClient(ClientConnection client, string reason)
        {
            bool removed = false;
            lock (sync) {
                removed = clients.Remove(client);
                if (ReferenceEquals(activeClient, client)) activeClient = null;
            }
            if (removed) {
                logger.InfoH(() => $"TSF pipe disconnected: client={client.Id}, reason={reason}");
            }
            client.Dispose();
        }

        private static string GetUserSidForPipeName()
        {
            try {
                return WindowsIdentity.GetCurrent().User.Value.Replace('-', '_');
            } catch {
                return "unknown";
            }
        }

        private static void WriteOperationRequest(Stream pipe, short operation, ulong compositionId, ulong sequence,
            string text, int caretOffset, int commitLength)
        {
            byte[] textBytes = Encoding.Unicode.GetBytes(text ?? "");
            using (var ms = new MemoryStream()) {
                WriteUInt64(ms, compositionId);
                WriteUInt64(ms, sequence);
                if (operation == MessageUpdateComposition) {
                    WriteInt32(ms, caretOffset);
                    WriteInt32(ms, textBytes.Length);
                    ms.Write(textBytes, 0, textBytes.Length);
                } else if (operation == MessageCommitComposition) {
                    WriteInt32(ms, commitLength);
                }
                byte[] payload = ms.ToArray();
                WriteHeader(pipe, operation, payload.Length);
                pipe.Write(payload, 0, payload.Length);
                pipe.Flush();
            }
        }

        private static void WriteHeader(Stream stream, short type, int payloadLength)
        {
            WriteInt32(stream, Magic);
            WriteInt16(stream, Version);
            WriteInt16(stream, type);
            WriteInt32(stream, payloadLength);
        }

        private static MessageHeader ReadHeader(Stream stream)
        {
            byte[] header = ReadExact(stream, 12);
            if (header == null) return null;
            int magic = BitConverter.ToInt32(header, 0);
            short version = BitConverter.ToInt16(header, 4);
            short type = BitConverter.ToInt16(header, 6);
            int payloadLength = BitConverter.ToInt32(header, 8);
            if (magic != Magic || (version != 1 && version != Version) || payloadLength < 0 || payloadLength > MaxPayloadLength) {
                throw new InvalidDataException($"invalid TSF pipe header: magic=0x{unchecked((uint)magic):X8}, version={version}, type={type}, payloadLength={payloadLength}");
            }
            return new MessageHeader(version, type, payloadLength);
        }

        private static byte[] ReadExact(Stream stream, int length)
        {
            var buffer = new byte[length];
            int offset = 0;
            while (offset < length) {
                int n = stream.Read(buffer, offset, length - offset);
                if (n <= 0) return null;
                offset += n;
            }
            return buffer;
        }

        private static void WriteInt16(Stream stream, short value)
        {
            var bytes = BitConverter.GetBytes(value);
            stream.Write(bytes, 0, bytes.Length);
        }

        private static void WriteInt32(Stream stream, int value)
        {
            var bytes = BitConverter.GetBytes(value);
            stream.Write(bytes, 0, bytes.Length);
        }

        private static void WriteUInt64(Stream stream, ulong value)
        {
            var bytes = BitConverter.GetBytes(value);
            stream.Write(bytes, 0, bytes.Length);
        }

        private sealed class MessageHeader
        {
            public MessageHeader(short version, short type, int payloadLength)
            {
                Version = version;
                Type = type;
                PayloadLength = payloadLength;
            }
            public short Version { get; }

            public short Type { get; }
            public int PayloadLength { get; }
        }

        private enum CommitOutcome
        {
            Success,
            ExplicitFailure,
            TimeoutAfterDispatch,
            Disconnected
        }

        private sealed class ClientConnection : IDisposable
        {
            private readonly TsfTextOutputSink owner;
            private readonly NamedPipeServerStream pipe;
            private readonly object commitLock = new object();
            private readonly object resultSync = new object();
            private readonly ManualResetEventSlim commitResultEvent = new ManualResetEventSlim(false);
            private Thread readerThread;
            private bool disposed;
            private bool waitingCommitResult;
            private bool commitResultReceived;
            private int commitHresult = HresultFail;
            private short expectedOperation;
            private ulong expectedCompositionId;
            private ulong expectedSequence;

            public ClientConnection(TsfTextOutputSink owner, NamedPipeServerStream pipe, int id)
            {
                this.owner = owner;
                this.pipe = pipe;
                Id = id;
            }

            public int Id { get; }
            public short ProtocolVersion { get; private set; }

            public bool IsConnected
            {
                get
                {
                    try {
                        return !disposed && pipe.IsConnected;
                    } catch {
                        return false;
                    }
                }
            }

            public void Start()
            {
                readerThread = new Thread(ReadLoop) {
                    IsBackground = true,
                    Name = $"AyaoriHIME TSF pipe reader {Id}"
                };
                readerThread.Start();
            }

            public CommitOutcome TryOperation(short operation, ulong compositionId, ulong sequence, string text,
                int caretOffset, int commitLength, int timeoutMs, out int hresult, out string error)
            {
                hresult = HresultFail;
                error = "";
                timeoutMs = Math.Max(1, timeoutMs);

                lock (commitLock) {
                    if (!IsConnected) {
                        error = "active TSF client is disconnected";
                        return CommitOutcome.Disconnected;
                    }

                    lock (resultSync) {
                        commitHresult = HresultFail;
                        commitResultReceived = false;
                        waitingCommitResult = true;
                        expectedOperation = operation;
                        expectedCompositionId = compositionId;
                        expectedSequence = sequence;
                        commitResultEvent.Reset();
                    }

                    try {
                        WriteOperationRequest(pipe, operation, compositionId, sequence, text, caretOffset, commitLength);
                    } catch (Exception ex) {
                        ClearWaitingCommitResult();
                        error = ex.Message;
                        return CommitOutcome.Disconnected;
                    }

                    if (!commitResultEvent.Wait(timeoutMs)) {
                        ClearWaitingCommitResult();
                        error = "TSF output timeout after dispatch";
                        return CommitOutcome.TimeoutAfterDispatch;
                    }

                    lock (resultSync) {
                        if (!commitResultReceived) {
                            waitingCommitResult = false;
                            error = "TSF pipe closed after dispatch";
                            return CommitOutcome.TimeoutAfterDispatch;
                        }
                        hresult = commitHresult;
                    }
                    return hresult == HresultOk ? CommitOutcome.Success : CommitOutcome.ExplicitFailure;
                }
            }

            public void Dispose()
            {
                if (disposed) return;
                disposed = true;
                try {
                    pipe.Dispose();
                } catch {
                }
                commitResultEvent.Set();
            }

            private void ReadLoop()
            {
                try {
                    while (!disposed) {
                        MessageHeader header = ReadHeader(pipe);
                        if (header == null) break;
                        byte[] payload = header.PayloadLength > 0 ? ReadExact(pipe, header.PayloadLength) : new byte[0];
                        if (payload == null) break;
                        if (!HandleMessage(header.Version, header.Type, payload)) break;
                    }
                } catch (ObjectDisposedException) {
                } catch (IOException ex) {
                    if (!disposed) logger.WarnH(() => $"TSF pipe read failed: client={Id}, {ex.Message}");
                } catch (Exception ex) {
                    if (!disposed) logger.WarnH(() => $"TSF pipe read failed: client={Id}, {ex.Message}");
                } finally {
                    owner.DisconnectClient(this, "pipe closed");
                }
            }

            private bool HandleMessage(short version, short type, byte[] payload)
            {
                if (ProtocolVersion == 0) ProtocolVersion = version;
                if (ProtocolVersion != version) return false;
                if (version == 1) {
                    switch (type) {
                        case MessageHello:
                            owner.OnClientHello(this);
                            return true;
                        case MessageFocusChanged:
                            owner.OnFocusChanged(this, ParseFocusPayload(payload));
                            return true;
                        case 5: // protocol v1 Bye
                            owner.OnClientBye(this);
                            return false;
                        default:
                            return true;
                    }
                }
                switch (type) {
                    case MessageHello:
                        owner.OnClientHello(this);
                        return true;
                    case MessageFocusChanged:
                        owner.OnFocusChanged(this, ParseFocusPayload(payload));
                        return true;
                    case MessageOperationResult:
                        HandleOperationResult(payload);
                        return true;
                    case MessageCompositionTerminated:
                        owner.OnCompositionTerminated(payload);
                        return true;
                    case MessageBye:
                        owner.OnClientBye(this);
                        return false;
                    default:
                        logger.WarnH(() => $"TSF pipe unknown message: client={Id}, type={type}, payloadLength={payload.Length}");
                        return true;
                }
            }

            private static bool ParseFocusPayload(byte[] payload)
            {
                return payload.Length < 4 || BitConverter.ToInt32(payload, 0) != 0;
            }

            private void HandleOperationResult(byte[] payload)
            {
                int operation = payload.Length >= 24 ? BitConverter.ToInt32(payload, 0) : 0;
                ulong id = payload.Length >= 24 ? BitConverter.ToUInt64(payload, 4) : 0;
                ulong sequence = payload.Length >= 24 ? BitConverter.ToUInt64(payload, 12) : 0;
                int hr = payload.Length >= 24 ? BitConverter.ToInt32(payload, 20) : HresultFail;
                lock (resultSync) {
                    if (!waitingCommitResult) {
                        logger.WarnH(() => $"TSF operation result without pending request: client={Id}, HRESULT=0x{unchecked((uint)hr):X8}");
                        return;
                    }
                    if (operation != expectedOperation || id != expectedCompositionId || sequence != expectedSequence) {
                        logger.WarnH(() => $"TSF operation result mismatch: client={Id}, operation={operation}, id={id}, sequence={sequence}");
                        return;
                    }
                    commitHresult = hr;
                    commitResultReceived = true;
                    waitingCommitResult = false;
                    commitResultEvent.Set();
                }
            }

            private void ClearWaitingCommitResult()
            {
                lock (resultSync) {
                    waitingCommitResult = false;
                    commitResultReceived = false;
                    commitHresult = HresultFail;
                    commitResultEvent.Set();
                }
            }
        }
    }
}
