/* Author: macote */

#ifndef REPORT_H_
#define REPORT_H_

#include "FileStream.h"
#include "StreamLineWriter.h"
#include <string>
#include <list>
#include <Windows.h>

class Report
{
public:
	void AddLine(const std::wstring line) { report_.push_back(line); };
	void Save(const std::wstring& reportpath) const
	{
		FileStream reportfile(reportpath, FileStream::Mode::Truncate);
		StreamLineWriter reportfilewriter(reportfile);
		for (auto& item : report_)
		{
			reportfilewriter.WriteLine(item);
		}
	};
	BOOL IsEmpty() { return report_.size() == 0; }
private:
	std::list<std::wstring> report_;
};

#endif /* REPORT_H_ */