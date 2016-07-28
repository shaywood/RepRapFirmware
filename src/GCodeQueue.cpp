/*
 * GCodeQueue.cpp
 *
 *  Created on: 22 Jun 2016
 *      Author: Christian
 */

#include "RepRapFirmware.h"
#include "GCodeQueue.h"


// GCodeQueue class

GCodeQueue::GCodeQueue() : freeItems(nullptr), queuedItems(nullptr)
{
	for(size_t i = 0; i < maxQueuedCodes; i++)
	{
		freeItems = new QueuedCode(freeItems);
	}
}

bool GCodeQueue::QueueCode(GCodeBuffer *gb)
{
	// Check for G-Codes
	bool queueCode = false;
	if (gb->Seen('G'))
	{
		const int code = gb->GetIValue();

		// Set active/standby temperatures
		queueCode = (code == 10 && gb->Seen('P'));
	}
	// Check for M-Codes
	else if (gb->Seen('M'))
	{
		const int code = gb->GetIValue();

		// Fan control
		queueCode |= (code == 106 || code == 107);

		// Set temperatures and return immediately
		queueCode |= (code == 104 || code == 140 || code == 141 || code == 144);

		// Display Message (LCD), Beep, RGB colour, Set servo position
		queueCode |= (code == 117 || code == 300 || code == 280 || code == 420);

		// Valve control
		queueCode |= (code == 126 || code == 127);

		// Set networking parameters, Emulation, Compensation, Z-Probe changes
		// File Uploads, Tool management
		queueCode |= (code == 540 || (code >= 550 && code <= 563));

		// Move, heater and auxiliary PWM control
		queueCode |= (code >= 566 && code <= 573);
	}

	// Does it make sense to queue this code?
	if (queueCode)
	{
		// Can we queue this code somewhere?
		char codeToQueue[GCODE_LENGTH];
		if (freeItems == nullptr)
		{
			// No - we've run out of free items
			strncpy(codeToQueue, queuedItems->code, GCODE_LENGTH);
			queueCode = false;

			// Release the first queued item so that it can be reused below
			QueuedCode *item = queuedItems;
			queuedItems = item->next;
			item->next = nullptr;
			freeItems = item;
		}

		// Unlink a free element and assign gb's code to it
		QueuedCode *code = freeItems;
		freeItems = code->next;
		code->AssignFrom(gb);
		
		// Append it to the list of queued codes
		QueuedCode *last = queuedItems;
		while (last->Next() != nullptr)
		{
			last = last->Next();
		}
		last->next = code;
		code->next = nullptr;

		// Overwrite the passed gb's content if we could not store its original code
		if (!queueCode)
		{
			for(size_t i = 0; i < GCODE_LENGTH; i++)
			{
				gb->Put(codeToQueue[i]);

				if (codeToQueue[i] == 0)
					break;
			}
		}
	}

	return queueCode;
}

bool GCodeQueue::FillBuffer(GCodeBuffer *gb)
{
	// Can this buffer be filled?
	if (queuedItems == nullptr || queuedItems->executeAtMove < reprap.GetMove()->GetCompletedMoves())
	{
		// No - stop here
		return false;
	}

	// Yes - load it into the passed GCodeBuffer instance
	QueuedCode *code = queuedItems;
	code->AssignTo(gb);
	//source = code->source;

	// Release this item again
	queuedItems = queuedItems->next;
	code->next = freeItems;
	freeItems = code;
	return true;
}

void GCodeQueue::PurgeEntries(unsigned int skippedMoves)
{
	unsigned int movesToDo = reprap.GetMove()->GetCompletedMoves() - skippedMoves;

	QueuedCode *item = queuedItems, *lastItem = nullptr;
	while (item != nullptr)
	{
		if (item->executeAtMove > movesToDo)
		{
			// Release this item
			QueuedCode *nextItem = item->Next();
			item->next = freeItems;
			freeItems = item;

			// Unlink it from the list
			if (lastItem == nullptr)
			{
				queuedItems = nextItem;
			}
			else
			{
				lastItem->next = nextItem;
			}
			item = nextItem;
		}
		else
		{
			lastItem = item;
			item = item->Next();
		}
	}
}

void GCodeQueue::Clear()
{
	while (queuedItems != nullptr)
	{
		QueuedCode *item = queuedItems;
		queuedItems = item->Next();
		item->next = freeItems;
		freeItems = item;
	}
}

void GCodeQueue::Diagnostics(MessageType mtype)
{
	reprap.GetPlatform()->MessageF(mtype, "Internal code queue is %s\n", (queuedItems == nullptr) ? "empty." : "not empty:");
	if (queuedItems != nullptr)
	{
		QueuedCode *item = queuedItems;
		size_t queueLength = 0;
		do {
			queueLength++;
			reprap.GetPlatform()->MessageF(mtype, "Queued '%s' for move %d\n", item->code, item->executeAtMove);
		} while ((item = item->Next()) != nullptr);
		reprap.GetPlatform()->MessageF(mtype, "%d of %d codes have been queued.\n", queueLength, maxQueuedCodes);
	}
}

// QueuedCode class

void QueuedCode::AssignFrom(GCodeBuffer *gb)
{
	source = gb;
	executeAtMove = reprap.GetMove()->GetScheduledMoves();
	toolNumberAdjust = gb->GetToolNumberAdjust();
	strncpy(code, gb->Buffer(), GCODE_LENGTH);
}

void QueuedCode::AssignTo(GCodeBuffer *gb)
{
	gb->SetToolNumberAdjust(toolNumberAdjust);
	gb->Put(code, strlen(code));
}
