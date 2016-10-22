//------------------------------------------------------------------------------------------------

#include "RepRapFirmware.h"
#include "FileStore.h"
#include "MassStorage.h"
#include "Platform.h"

uint32_t FileStore::longestWriteTime = 0;

FileStore::FileStore(Platform* p) : platform(p)
{
}

void FileStore::Init()
{
	inUse = false;
	writing = false;
	lastBufferEntry = 0;
	openCount = 0;
	closeRequested = false;
}

// Invalidate the file if it uses the specified FATFS object
void FileStore::Invalidate(const FATFS *fs)
{
	if (file.fs == fs)
	{
		Init();
		file.fs = nullptr;
	}
}

// Return true if the file is open on the specified file system
bool FileStore::IsOpenOn(const FATFS *fs) const
{
	return openCount != 0 && file.fs == fs;
}

// Open a local file (for example on an SD card).
// This is protected - only Platform can access it.
bool FileStore::Open(const char* directory, const char* fileName, bool write)
{
	const char* location = (directory != nullptr)
							? platform->GetMassStorage()->CombineName(directory, fileName)
								: fileName;
	writing = write;

	FRESULT openReturn = f_open(&file, location, (writing) ?  FA_CREATE_ALWAYS | FA_WRITE : FA_OPEN_EXISTING | FA_READ);
	if (openReturn != FR_OK)
	{
		// We no longer report an error if opening a file in read mode fails unless debugging is enabled, because sometimes that is quite normal.
		// It is up to the caller to report an error if necessary.
		if (reprap.Debug(modulePlatform))
		{
			platform->MessageF(GENERIC_MESSAGE, "Can't open %s to %s, error code %d\n", location, (writing) ? "write" : "read", openReturn);
		}
		return false;
	}

	inUse = true;
	openCount = 1;
	return true;
}

void FileStore::Duplicate()
{
	if (!inUse)
	{
		platform->Message(GENERIC_MESSAGE, "Error: Attempt to dup a non-open file.\n");
		return;
	}
	irqflags_t flags = cpu_irq_save();
	++openCount;
	cpu_irq_restore(flags);
}

// This may be called from an ISR, in which case we need to defer the close
bool FileStore::Close()
{
	if (inInterrupt())
	{
		if (!inUse)
		{
			return false;
		}

		irqflags_t flags = cpu_irq_save();
		if (openCount > 1)
		{
			--openCount;
		}
		else
		{
			closeRequested = true;
		}
		cpu_irq_restore(flags);
		return true;
	}

	if (!inUse)
	{
		platform->Message(GENERIC_MESSAGE, "Error: Attempt to close a non-open file.\n");
		return false;
	}

	irqflags_t flags = cpu_irq_save();
	--openCount;
	bool leaveOpen = (openCount != 0);
	cpu_irq_restore(flags);

	if (leaveOpen)
	{
		return true;
	}

	bool ok = true;
	if (writing)
	{
		ok = Flush();
	}

	FRESULT fr = f_close(&file);
	inUse = false;
	writing = false;
	closeRequested = false;
	return ok && fr == FR_OK;
}

bool FileStore::Seek(FilePosition pos)
{
	if (!inUse)
	{
		platform->Message(GENERIC_MESSAGE, "Error: Attempt to seek on a non-open file.\n");
		return false;
	}
	FRESULT fr = f_lseek(&file, pos);
	return fr == FR_OK;
}

FilePosition FileStore::Position() const
{
	return file.fptr;
}

#if 0	// not currently used
bool FileStore::GoToEnd()
{
	return Seek(Length());
}
#endif

FilePosition FileStore::Length() const
{
	if (!inUse)
	{
		platform->Message(GENERIC_MESSAGE, "Error: Attempt to size non-open file.\n");
		return 0;
	}
	return file.fsize;
}

float FileStore::FractionRead() const
{
	FilePosition len = Length();
	if (len == 0)
	{
		return 0.0;
	}

	return (float)Position() / (float)len;
}

// Single character read
bool FileStore::Read(char& b)
{
	return Read(&b, sizeof(char));
}

// Returns the number of bytes read or -1 if the read process failed
int FileStore::Read(char* extBuf, size_t nBytes)
{
	if (!inUse)
	{
		platform->Message(GENERIC_MESSAGE, "Error: Attempt to read from a non-open file.\n");
		return -1;
	}

	UINT bytes_read;
	FRESULT readStatus = f_read(&file, extBuf, nBytes, &bytes_read);
	if (readStatus != FR_OK)
	{
		platform->Message(GENERIC_MESSAGE, "Error: Cannot read file.\n");
		return -1;
	}
	return (int)bytes_read;
}

bool FileStore::Write(char b)
{
	return Write(&b, sizeof(char));
}

bool FileStore::Write(const char* b)
{
	return Write(b, strlen(b));
}

bool FileStore::Write(const char *s, size_t len)
{
	if (!inUse)
	{
		platform->Message(GENERIC_MESSAGE, "Error: Attempt to write block to a non-open file.\n");
		return false;
	}

	size_t bytesWritten;
	uint32_t time = micros();

	FRESULT writeStatus = f_write(&file, s, len, &bytesWritten);
	time = micros() - time;
	if (time > longestWriteTime)
	{
		longestWriteTime = time;
	}
	if ((writeStatus != FR_OK) || (bytesWritten != len))
	{
		platform->Message(GENERIC_MESSAGE, "Error: Cannot write to file. Disc may be full.\n");
		return false;
	}
	return true;
}

bool FileStore::Flush()
{
	if (!inUse)
	{
		platform->Message(GENERIC_MESSAGE, "Error: Attempt to flush a non-open file.\n");
		return false;
	}
	return f_sync(&file) == FR_OK;
}

float FileStore::GetAndClearLongestWriteTime()
{
	float ret = (float)longestWriteTime/1000.0;
	longestWriteTime = 0;
	return ret;
}

#if 0	// not currently used

// Provide a cluster map for fast seeking. Needs _USE_FASTSEEK defined as 1 in conf_fatfs to make any difference.
// The first element of the table must be set to the total number of 32-bit entries in the table before calling this.
bool FileStore::SetClusterMap(uint32_t tbl[])
{
	if (!inUse)
	{
		platform->Message(GENERIC_MESSAGE, "Error: Attempt to set cluster map for a non-open file.\n");
		return false;
	}

	file.cltbl = tbl;
	FRESULT ret = f_lseek(&file, CREATE_LINKMAP);
//	debugPrintf("ret %d need %u\n", (int)ret, tbl[0]);
	return ret == FR_OK;
}

#endif

// End
