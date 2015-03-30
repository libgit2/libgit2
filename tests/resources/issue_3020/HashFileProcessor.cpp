/* Author: macote */

#include "HashFileProcessor.h"

HashFileProcessor::ProcessResult HashFileProcessor::ProcessTree()
{
	auto result = ProcessResult::Success;
	newfilesupdated_ = FALSE;
	if (mode_ == HashFileProcessor::Mode::Verify || mode_ == HashFileProcessor::Mode::Update)
	{
		try
		{
			hashfile_.Load(hashfilename_);
		}
		catch (...)
		{
			result = ProcessResult::CouldNotOpenHashFile;
			return result;
		}
	}
	FileTree filetree(basepath_, *this);
	filetree.Process();
	if (mode_ == HashFileProcessor::Mode::Create)
	{
		if (hashfile_.IsEmpty())
		{
			result = ProcessResult::NoFileToProcess;
		}
		else if (!report_.IsEmpty())
		{
			result = ProcessResult::ErrorsOccurredWhileProcessing;
		}
		else
		{
			hashfile_.Save(hashfilename_);
		}
	}
	else if (mode_ == HashFileProcessor::Mode::Verify || mode_ == HashFileProcessor::Mode::Update)
	{
		if (!hashfile_.IsEmpty())
		{
			for (auto& relativefilepath : hashfile_.GetFilePaths())
			{
				// TODO: replace hardcoded text
				report_.AddLine(L"Missing             : " + relativefilepath);
			}
			result = ProcessResult::FilesAreMissing;
		}
		else if (!report_.IsEmpty())
		{
			result = ProcessResult::ErrorsOccurredWhileProcessing;
		}
		else if (mode_ == HashFileProcessor::Mode::Update)
		{
			if (!newfilesupdated_)
			{
				result = ProcessResult::NothingToUpdate;
			}
			else
			{
				// replace old hash file
				DeleteFileW(hashfilename_.c_str());
				newhashfile_.Save(hashfilename_);
			}
		}
	}
	return result;
}

void HashFileProcessor::ProcessFile(const std::wstring& filepath)
{
	if (lstrcmpiW(appfilepath_.c_str(), filepath.c_str()) == 0 || lstrcmpiW(hashfilename_.c_str(), filepath.c_str()) == 0)
	{
		// skip self and current hash file
		return;
	}
	auto relativefilepath = filepath.substr(basepath_.length(), filepath.length());
	const FileEntry& fileentry = hashfile_.GetFileEntry(relativefilepath);
	if (mode_ == HashFileProcessor::Mode::Verify)
	{
		if (&fileentry == &HashFile::kFileEntryNull)
		{
			report_.AddLine(L"Unknown             : " + relativefilepath);
			return;
		}
	}
	else if (mode_ == HashFileProcessor::Mode::Update)
	{
		if (&fileentry == &HashFile::kFileEntryNull)
		{
			newhashfile_.AddFileEntry(fileentry.filepath(), fileentry.size(), fileentry.digest());
			hashfile_.RemoveFileEntry(relativefilepath);
			return;
		}
	}
	LARGE_INTEGER size;
	size.QuadPart = 0;
	auto file = CreateFileW(filepath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file != INVALID_HANDLE_VALUE)
	{
		GetFileSizeEx(file, &size);
		CloseHandle(file);
	}
	else
	{
		report_.AddLine(L"Error opening file  : " + relativefilepath);
		if (mode_ == HashFileProcessor::Mode::Verify)
		{
			hashfile_.RemoveFileEntry(relativefilepath);
		}
		return;
	}
	if (mode_ == HashFileProcessor::Mode::Verify)
	{
		if (size.QuadPart != fileentry.size().QuadPart)
		{
			report_.AddLine(L"Incorrect file size : " + relativefilepath);
			hashfile_.RemoveFileEntry(relativefilepath);
			return;
		}
	}
	auto filehash = FileHashFactory::Create(hashtype_, filepath);
	if (progressevent_ != nullptr)
	{
		hfppea_.bytesprocessed.QuadPart = 0;
		hfppea_.relativefilepath = relativefilepath;
		progressevent_(hfppea_);
		filehash->SetBytesProcessedEventHandler([this](FileHashBytesProcessedEventArgs fhbea) {
			this->hfppea_.bytesprocessed = fhbea.bytesprocessed;
			this->progressevent_(hfppea_);
		}, bytesprocessednotificationblocksize_);
	}
	filehash->Compute();
	std::wstring digest = filehash->digest();
	if (mode_ == HashFileProcessor::Mode::Create)
	{
		hashfile_.AddFileEntry(relativefilepath, size, digest);
	}
	else if (mode_ == HashFileProcessor::Mode::Update)
	{
		newhashfile_.AddFileEntry(relativefilepath, size, digest);
		newfilesupdated_ = TRUE;
	}
	else if (mode_ == HashFileProcessor::Mode::Verify)
	{
		if (size.QuadPart != fileentry.size().QuadPart)
		{
			report_.AddLine(L"Incorrect file size : " + relativefilepath);
		}
		else if (digest != fileentry.digest())
		{
			report_.AddLine(L"Incorrect hash      : " + relativefilepath);
		}
		hashfile_.RemoveFileEntry(relativefilepath);
	}
}
