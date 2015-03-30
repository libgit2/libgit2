/* Author: macote */

#include "HashFile.h"
#include "FileTree.h"
#include "HashType.h"
#include "FileHashFactory.h"
#include "Report.h"

struct HashFileProcessorProgressEventArgs
{
	std::wstring relativefilepath;
	LARGE_INTEGER bytesprocessed;
};

class HashFileProcessor : public IFileTreeAction
{
public:
	static const DWORD kDefaultBytesProcessedNotificationBlockSize = 1048576;
public:
	enum class Mode
	{
		Create,
		Update,
		Verify,
		Undefined
	};
	enum class ProcessResult
	{
		FilesAreMissing,
		NothingToUpdate,
		CouldNotOpenHashFile,
		ErrorsOccurredWhileProcessing,
		NoFileToProcess,
		Success
	};
	HashFileProcessor(Mode mode, HashType hashtype, std::wstring hashfilename, std::wstring appfilepath, std::wstring basepath)
		: mode_(mode), hashtype_(hashtype), hashfilename_(hashfilename), appfilepath_(appfilepath), basepath_(basepath)
	{ 
		progressevent_ = nullptr;
	};
	ProcessResult ProcessTree();
	void ProcessFile(const std::wstring& filepath);
	void SaveReport(const std::wstring& reportpath) const { report_.Save(reportpath); }
	void SetProgressEventHandler(std::function<void(HashFileProcessorProgressEventArgs)> handler)
	{
		SetProgressEventHandler(handler, kDefaultBytesProcessedNotificationBlockSize);
	}
	void SetProgressEventHandler(std::function<void(HashFileProcessorProgressEventArgs)> handler,
		const DWORD bytesprocessednotificationblocksize)
	{
		progressevent_ = handler;
		bytesprocessednotificationblocksize_ = bytesprocessednotificationblocksize;
	}
private:
	Mode mode_;
	HashType hashtype_;
	HashFile hashfile_;
	HashFile newhashfile_;
	std::wstring hashfilename_;
	std::wstring appfilepath_;
	std::wstring basepath_;
	BOOL newfilesupdated_ = FALSE;
	Report report_;
	HashFileProcessorProgressEventArgs hfppea_;
	DWORD bytesprocessednotificationblocksize_;
	std::function<void(HashFileProcessorProgressEventArgs)> progressevent_;
};
