/*
 * GCodeInput.cpp
 *
 *  Created on: 16 Sep 2016
 *      Author: Christian
 */

#include "RepRapFirmware.h"
#include "GCodeInput.h"


// G-code input class for wrapping around Stream-based hardware ports


void StreamGCodeInput::Reset()
{
	while (device.available() > 0)
	{
		device.read();
	}
}

bool StreamGCodeInput::FillBuffer(GCodeBuffer *gb)
{
	size_t bytesToPass = min<size_t>(device.available(), GCODE_LENGTH);
	for(size_t i = 0; i < bytesToPass; i++)
	{
		char c = static_cast<char>(device.read());

		if (gb->WritingFileDirectory() == reprap.GetPlatform()->GetWebDir())
		{
			// HTML uploads are handled by the GCodes class
			reprap.GetGCodes()->WriteHTMLToFile(c, gb);
		}
		else if (gb->Put(c))
		{
			// Check if we can finish a file upload
			if (gb->WritingFileDirectory() != nullptr)
			{
				reprap.GetGCodes()->WriteGCodeToFile(gb);
				gb->SetFinished(true);
			}

			// Code is complete, stop here
			return true;
		}
	}

	return false;
}

size_t StreamGCodeInput::BytesCached() const
{
	return device.available();
}


// Dynamic G-code input class for caching codes from software-defined sources


RegularGCodeInput::RegularGCodeInput(bool removeComments): stripComments(removeComments),
	state(GCodeInputState::idle), buffer(reinterpret_cast<char * const>(buf32)), writingPointer(0), readingPointer(0)
{
}

void RegularGCodeInput::Reset()
{
	state = GCodeInputState::idle;
	writingPointer = readingPointer = 0;
}

bool RegularGCodeInput::FillBuffer(GCodeBuffer *gb)
{
	size_t bytesToPass = min<size_t>(BytesCached(), GCODE_LENGTH);
	for(size_t i = 0; i < bytesToPass; i++)
	{
		// Get a char from the buffer
		char c = buffer[readingPointer++];
		if (readingPointer == GCodeInputBufferSize)
		{
			readingPointer = 0;
		}

		// Pass it on to the GCodeBuffer
		if (gb->WritingFileDirectory() == reprap.GetPlatform()->GetWebDir())
		{
			// HTML uploads are handled by the GCodes class
			reprap.GetGCodes()->WriteHTMLToFile(c, gb);
		}
		else if (gb->Put(c))
		{
			// Check if we can finish a file upload
			if (gb->WritingFileDirectory() != nullptr)
			{
				reprap.GetGCodes()->WriteGCodeToFile(gb);
				gb->SetFinished(true);
			}

			// Code is complete, stop here
			return true;
		}
	}

	return false;
}

size_t RegularGCodeInput::BytesCached() const
{
	if (writingPointer >= readingPointer)
	{
		return writingPointer - readingPointer;
	}
	return GCodeInputBufferSize - readingPointer + writingPointer;
}

void RegularGCodeInput::Put(const char c)
{
	if (BufferSpaceLeft() == 0)
	{
		// Don't let the buffer overflow if we run out of space
		return;
	}

	// Check for M112 (emergency stop) while receiving new characters
	switch (state)
	{
		case GCodeInputState::idle:
			if (c <= ' ')
			{
				// ignore whitespaces at the beginning
				return;
			}

			state = (c == 'M') ? GCodeInputState::doingMCode : GCodeInputState::doingCode;
			break;


		case GCodeInputState::doingCode:
			if (stripComments && c == ';')
			{
				// ignore comments if possible
				state = GCodeInputState::inComment;
			}
			break;

		case GCodeInputState::inComment:
			if (c == 0 || c == '\r' || c == '\n')
			{
				state = GCodeInputState::idle;
			}
			break;

		case GCodeInputState::doingMCode:
		case GCodeInputState::doingMCode1:
			if (c == '1')
			{
				state = (state == GCodeInputState::doingMCode) ? GCodeInputState::doingMCode1 : GCodeInputState::doingMCode11;
			}
			else
			{
				state = GCodeInputState::doingCode;
			}
			break;

		case GCodeInputState::doingMCode11:
			if (c == '2')
			{
				state = GCodeInputState::doingMCode112;
				break;
			}
			state = GCodeInputState::doingCode;
			break;

		case GCodeInputState::doingMCode112:
			if (c <= ' ' || c == ';')
			{
				// Emergency stop requested - perform it now
				reprap.EmergencyStop();
				reprap.GetGCodes()->Reset();

				// But don't run it twice
				readingPointer = (readingPointer + 5) % GCodeInputBufferSize;
				state = GCodeInputState::idle;
				return;
			}

			state = GCodeInputState::doingCode;
			break;
	}

	// Feed another character into the buffer
	if (state != GCodeInputState::inComment)
	{
		buffer[writingPointer++] = c;
		if (writingPointer == GCodeInputBufferSize)
		{
			writingPointer = 0;
		}
	}
}

void RegularGCodeInput::Put(const char *buf)
{
	Put(buf, strlen(buf));
}

void RegularGCodeInput::Put(const char *buf, size_t len)
{
	if (len > BufferSpaceLeft())
	{
		// Don't cache this if we don't have enough space left
		return;
	}

	for(size_t i = 0; i < len; i++)
	{
		Put(buf[i]);
	}
	Put((char)0);
}

size_t RegularGCodeInput::BufferSpaceLeft() const
{
	if (readingPointer > writingPointer)
	{
		return readingPointer - writingPointer;
	}
	return GCodeInputBufferSize - writingPointer + readingPointer;
}


// File-based G-code input source


// Reset this input. Should also be called when the associated file is being closed
void FileGCodeInput::Reset()
{
	lastFile = nullptr;
	RegularGCodeInput::Reset();
}

// Read another chunk of G-codes from the file and return true if more data is available
bool FileGCodeInput::ReadFromFile(FileData &file)
{
	const size_t bytesCached = BytesCached();

	// Keep track of the last file we read from
	if (lastFile != nullptr && lastFile != file.f)
	{
		if (bytesCached > 0)
		{
			// Rewind back to the right position so we can resume at the right position later.
			// This may be necessary when nested macros are executed.
			lastFile->Seek(lastFile->Position() - bytesCached);
		}

		RegularGCodeInput::Reset();
	}
	lastFile = file.f;

	// Read more from the file
	if (file.IsLive() && bytesCached < GCodeInputFileReadThreshold)
	{
		// Reset the read+write pointers for better performance if possible
		if (readingPointer == writingPointer)
		{
			readingPointer = writingPointer = 0;
		}

		/*** This code isn't working yet ***
		 * If the file.Read() call is replaced by a dumb routine to fill up the buffer with lots of "M83\n",
		 * the firmware loads normally, however if one of the two blocks below is activated, doingFileMacro
		 * in the GCodes class is set to "false" without any good reason - possibly by a race condition and/or
		 * by a memory fault, and despite several debugging attempts I (chrishamm) have not yet been able to
		 * figure out why this is happening. In addition adding debug messages with some delay() calls seems
		 * to "resolve" the problem which makes the whole issue even worse to diagnose... */

#if 1
		// Read blocks with sizes multiple of 4 for HSMCI efficiency
		uint32_t readBuffer32[(GCodeInputBufferSize + 3) / 4];
		char * const readBuffer = reinterpret_cast<char * const>(readBuffer32);

		int bytesRead = file.Read(readBuffer, BufferSpaceLeft() & (~3));
		if (bytesRead > 0)
		{
			if (writingPointer + bytesRead <= GCodeInputBufferSize)
			{
				memcpy(buffer + writingPointer, readBuffer, bytesRead);
				writingPointer += bytesRead;
			}
			else
			{
				size_t bytesAtEnd = GCodeInputBufferSize - writingPointer;
				memcpy(buffer + writingPointer, readBuffer, bytesAtEnd);
				writingPointer = bytesRead - bytesAtEnd;
				memcpy(buffer, readBuffer + bytesAtEnd, writingPointer);
			}

			return true;
		}
#else
#if 0
		// Realign the remaining data and read more from the file
		if (readingPointer != 0)
		{
			// Any data left at the end of the circular buffer?
			if (writingPointer < readingPointer)
			{
				// Yes - realign both segments at the end and at the beginning
				size_t bytesAtEnd = GCodeInputBufferSize - readingPointer;
				memmove(buffer + bytesAtEnd, buffer, bytesCached - bytesAtEnd);
				memmove(buffer, buffer + readingPointer, bytesAtEnd);
			}
			else
			{
				// No - we can realign the whole segment in one go
				memmove(buffer, buffer + readingPointer, bytesCached);
			}

			readingPointer = 0;
			writingPointer = bytesCached;
		}

		// Read blocks with sizes multiple of 4 for HSMCI efficiency
		int bytesRead = file.Read(buffer + writingPointer, BufferSpaceLeft() & (~3));
		if (bytesRead > 0)
		{
			writingPointer += bytesRead;
			return true;
		}
#else
		// TEST CASE: Create a dummy buffer and fill it up with lots of 'M83\n' codes.
		// This appears to be working even though the passages above create problems.
		// To verify it does work, debug should be enabled on boot in RepRap.cpp.
		uint32_t readBuffer32[(GCodeInputBufferSize + 3) / 4];
		char * const readBuffer = reinterpret_cast<char * const>(readBuffer32);

		size_t bytesRead = BufferSpaceLeft() & (~3);
		for(size_t i = 0; i < bytesRead / 4; i++)
		{
			readBuffer[i * 4] = 'M';
			readBuffer[i * 4 + 1] = '8';
			readBuffer[i * 4 + 2] = '3';
			readBuffer[i * 4 + 3] = '\n';
		}

		if (bytesRead > 0)
		{
			if (writingPointer + bytesRead <= GCodeInputBufferSize)
			{
				memcpy(buffer + writingPointer, readBuffer, bytesRead);
				writingPointer += bytesRead;
			}
			else
			{
				size_t bytesAtEnd = GCodeInputBufferSize - writingPointer;
				memcpy(buffer + writingPointer, readBuffer, bytesAtEnd);
				writingPointer = bytesRead - bytesAtEnd;
				memcpy(buffer, readBuffer + bytesAtEnd, writingPointer);
			}

			return true;
		}
#endif
#endif
	}

	return bytesCached > 0;
}

