#include "string_utils.h"
#include "path_utils.h"

#include "Logger.h"

namespace Reporting {
	//-----------------------------------------------------------------------------
	// ファイルへの書き込みを行うクラス.
	class FileWriter
	{
	private:
		HANDLE m_hFile;
		String m_logfilepath;
		bool m_fileCreationFailed = false;

	public:
		FileWriter(const String& logfilepath)
			: m_hFile(INVALID_HANDLE_VALUE),
			m_logfilepath(logfilepath)
		{
		}

		~FileWriter() {
			Close();
		}

	public:
		// ログファイルへの書き込み
		void WriteLog(const char* logmsg) {
			if (m_hFile == INVALID_HANDLE_VALUE && !m_fileCreationFailed) {
				openFile();
			}

			DWORD dwWritten = 0;
			WriteFile(m_hFile, (LPCVOID)logmsg, lstrlenA(logmsg), &dwWritten, NULL);

			//flushLog();
		}

		// ログファイルへの書き込み
		void WriteLog(const std::string& logmsg) {
			WriteLog(logmsg.c_str());
		}

		void Close() {
			if (m_hFile != NULL && m_hFile != INVALID_HANDLE_VALUE) {
				FlushFileBuffers(m_hFile);
				CloseHandle(m_hFile);
				m_hFile = INVALID_HANDLE_VALUE;
			}
		}

		// フラッシュ
		void flushLog() {
			if (INVALID_HANDLE_VALUE != m_hFile) {
				FlushFileBuffers(m_hFile);
			}
		}

	private:
		// ログファイルの作成
		void openFile()
		{
			try {
				HANDLE hRet = create_file(m_logfilepath);
				if (hRet == INVALID_HANDLE_VALUE) {
					String dirpath = utils::getParentDirPath(m_logfilepath);
					if (!dirpath.empty()) {
						if (utils::makeDirectory(dirpath)) {
							hRet = create_file(m_logfilepath);
						}
					}
					if (hRet == NULL || hRet == INVALID_HANDLE_VALUE) {
						m_fileCreationFailed = true;
						return;
					}
				}
				SetFilePointer(hRet, 0, 0, FILE_END);
				m_hFile = hRet;
			}
			catch (...) {
				throw;
			}
		}

		// ファイル作成
		HANDLE create_file(const String& filepath)
		{
			DWORD dwFlags =
				FILE_ATTRIBUTE_ARCHIVE |
				FILE_FLAG_SEQUENTIAL_SCAN |
				FILE_FLAG_WRITE_THROUGH;

			return ::CreateFile(filepath.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
				NULL, OPEN_ALWAYS, dwFlags, NULL);
		}

	}; // class FileWriter

	// FileWriterのSingleton
    std::unique_ptr<FileWriter> Logger::fileWriterPtr;

	bool Logger::initializeFileWriter() {
		if (!fileWriterPtr) {
			if (Logger::_logFilename.empty()) return false;
			fileWriterPtr.reset(new FileWriter(Logger::_logFilename));
		}
		return true;
	}

	std::string getDatetimeStr() {
		SYSTEMTIME st;
		GetLocalTime(&st);
		return std::format("{:04d}/{:02d}/{:02d} {:02d}:{:02d}:{:02d}.{:03d}",
			st.wYear,
			st.wMonth,
			st.wDay,
			st.wHour,
			st.wMinute,
			st.wSecond,
			st.wMilliseconds);
	}

	std::string formatMessage(const std::string& level, const std::string& className, const std::string& method, int line, StringRef msg) {
		return std::format("{} {} [{}.{}({})] {}\n", getDatetimeStr(), level, className, method, line, utils::utf8_encode(msg));
	}

	void _write_log(FileWriter& fw, const std::string& level, const std::string& className, const std::string& method, int line, StringRef msg) {
		fw.WriteLog(formatMessage(level, className, method, line, msg));
	}

	static const int QUEUE_SIZE = 20000;
	static const int QUEUE_EXTRA_SIZE = 2000;

	void appendLog(Deque<std::string>& queue, const std::string& msg) {
		if (queue.size() > QUEUE_SIZE + QUEUE_EXTRA_SIZE) queue.erase(queue.begin(), queue.begin() + QUEUE_EXTRA_SIZE);
		queue.push_back(msg);
	}

	void _write_log(Deque<std::string>& queue, const std::string& level, const std::string& className, const std::string& method, int line, StringRef msg) {
		appendLog(queue, formatMessage(level, className, method, line, msg));
	}

	//-----------------------------------------------------------------------------
	int Logger::_saveLevel = 0;
	int Logger::_logLevel = Logger::LogLevelWarn;

	String Logger::_logFilename;

	Deque<std::string> Logger::_traceLogQueue;

	void Logger::SetLogLevel(int logLevel) {
		_logLevel = logLevel;
		if (logLevel <= LogLevelWarn) Close();
	}

	void Logger::SetLogFilename(StringRef logFilename) {
		_logFilename = logFilename;
	}

	void Logger::SaveLog() {
		if (initializeFileWriter()) {
			while (!_traceLogQueue.empty()) {
				fileWriterPtr->WriteLog(_traceLogQueue.front());
				_traceLogQueue.pop_front();
			}
			fileWriterPtr->flushLog();
		}
	}

	void Logger::Close() {
		//_logFilename.clear();
		fileWriterPtr.reset();
	}

	void Logger::WriteLog(const std::string& msg) {
		//if (initializeFileWriter()) {
		//	fileWriterPtr->WriteLog(msg);
		//}
		appendLog(_traceLogQueue, msg);
	}

	void Logger::WriteLog(const String& msg) {
		WriteLog(utils::utf8_encode(msg));
	}

	void Logger::writeLogToFile(const std::string& level, const std::string& method, const std::string& /*file*/, int line, StringRef msg)
	{
		if (initializeFileWriter()) {
			if (msg.size() > 0 && msg[0] == '\n') {
				std::string newlines;
				size_t n = 0;
				while (msg.size() > 0 && n < msg.size() && msg[n] == '\n') {
					newlines.push_back('\n');
					++n;
				}
				fileWriterPtr->WriteLog(newlines);
				if (n < msg.size()) _write_log(*fileWriterPtr, level, _className, method, line, msg.substr(n));
			} else {
				_write_log(*fileWriterPtr, level, _className, method, line, msg);
			}
			if (LogLevel() <= LogLevelWarn) Close();
		}
	}

	void Logger::writeLogToQueue(const std::string& level, const std::string& method, const std::string& /*file*/, int line, StringRef msg)
	{
        if (msg.size() > 0 && msg[0] == '\n') {
            std::string newlines;
            size_t n = 0;
            while (msg.size() > 0 && n < msg.size() && msg[n] == '\n') {
                newlines.push_back('\n');
                ++n;
            }
            appendLog(_traceLogQueue, newlines);
            if (n < msg.size()) _write_log(_traceLogQueue, level, _className, method, line, msg.substr(n));
        } else {
            _write_log(_traceLogQueue, level, _className, method, line, msg);
        }
	}
}
