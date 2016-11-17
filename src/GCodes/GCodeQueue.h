/*
 * GCodeQueue.h
 *
 *  Created on: 22 Jun 2016
 *      Author: Christian
 */
#ifndef GCODEQUEUE_H
#define GCODEQUEUE_H

#include "GCodes.h"

const size_t maxQueuedCodes = 8;				// How many codes can be queued?

class QueuedCode;

class GCodeQueue
{
	public:
		GCodeQueue();
		
		bool QueueCode(GCodeBuffer &gb);							// Attempt to queue a G-code and return true on success
		bool FillBuffer(GCodeBuffer *gb);							// If there is another move to execute at this time, fill a buffer
		void PurgeEntries(unsigned int skippedMoves);				// Remove stored codes if the print is being paused
		void Clear();												// Clean up all stored codes

		void Diagnostics(MessageType mtype);

	private:
		QueuedCode *freeItems;
		QueuedCode *queuedItems;
};

class QueuedCode
{
	public:
		friend class GCodeQueue;

		QueuedCode(QueuedCode *n) : next(n) { }
		QueuedCode *Next() const { return next; }

	private:
		QueuedCode *next;

		char code[GCODE_LENGTH];
		unsigned int executeAtMove;
		int toolNumberAdjust;

		void AssignFrom(GCodeBuffer &gb);
		void AssignTo(GCodeBuffer *gb);
};

#endif
