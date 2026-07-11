/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////


#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameNetwork/NetCommandList.h"
#include "GameNetwork/networkutil.h"

/**
 * Constructor.
 */
NetCommandList::NetCommandList() {
	m_first = nullptr;
	m_last = nullptr;
	m_lastMessageInserted = nullptr;
}

/**
 * Destructor.
 */
NetCommandList::~NetCommandList() {
	reset();
}

/**
 * Append the given list of commands to this list.
 */
void NetCommandList::appendList(NetCommandList *list) {
	if (list == nullptr) {
		return;
	}

	// Need to do it this way because of the reference counting that needs to happen in appendMessage.
	NetCommandRef *msg = list->getFirstMessage();
	NetCommandRef *next = nullptr;
	while (msg != nullptr) {
		next = msg->getNext();
		NetCommandRef *temp = addMessage(msg->getCommand());
		if (temp != nullptr) {
			temp->setRelay(msg->getRelay());
		}

		msg = next;
	}
}

/**
 * Return the first message in this list.
 */
NetCommandRef *NetCommandList::getFirstMessage() {
	return m_first;
}

/**
 * Remove the given message from this list.
 */
void NetCommandList::removeMessage(NetCommandRef *msg) {
	if (m_lastMessageInserted == msg) {
		m_lastMessageInserted = msg->getNext();
	}

	if (msg->getPrev() != nullptr) {
		msg->getPrev()->setNext(msg->getNext());
	}
	if (msg->getNext() != nullptr) {
		msg->getNext()->setPrev(msg->getPrev());
	}

	if (msg == m_first) {
		m_first = msg->getNext();
	}
	if (msg == m_last) {
		m_last = msg->getPrev();
	}

	msg->setNext(nullptr);
	msg->setPrev(nullptr);
}

/**
 * Initialize the list.
 */
void NetCommandList::init() {
	reset();
}

/**
 * Reset the contents of this list.
 */
void NetCommandList::reset() {
	NetCommandRef *temp = m_first;
	while (m_first != nullptr) {
		temp = m_first->getNext();
		m_first->setNext(nullptr);
		m_first->setPrev(nullptr);
		deleteInstance(m_first);
		m_first = temp;
	}
	m_last = nullptr;
	m_lastMessageInserted = nullptr;
}

static bool isCommandIdNewer(UnsignedShort newVal, UnsignedShort oldVal)
{
#if RETAIL_COMPATIBLE_NETWORKING
	return newVal > oldVal;
#else
	// TheSuperHackers @bugfix Caball009 14/06/2026 Ensure messages are sorted
	// chronologically by including a command id overflow check.
	const UnsignedShort diff = newVal - oldVal;
	return diff != 0 && diff < 0x8000;
#endif
}

/**
 * Insert sorts msg.  Assumes that all the previous message inserts were done using this function.
 * The message is sorted in based first on command type, then player id, and then command id.
 */
NetCommandRef * NetCommandList::addMessage(NetCommandMsg *cmdMsg) {
	if (cmdMsg == nullptr) {
		DEBUG_ASSERTCRASH(cmdMsg != nullptr, ("NetCommandList::addMessage - command message was null"));
		return nullptr;
	}

//	UnsignedInt id = cmdMsg->getID();

	NetCommandRef *msg = NEW_NETCOMMANDREF(cmdMsg);

	if (m_first == nullptr) {
		// this is the first node, so we don't have to worry about ordering it.
		m_first = msg;
		m_last = msg;
		m_lastMessageInserted = msg;
		return msg;
	}

	if (m_lastMessageInserted != nullptr) {
		// Messages that are inserted in order should just be put in one right after the other.
		// So saving the placement of the last message inserted can give us a huge boost in
		// efficiency.
		NetCommandRef *theNext = m_lastMessageInserted->getNext();
		if ((m_lastMessageInserted->getCommand()->getNetCommandType() == msg->getCommand()->getNetCommandType()) &&
			(m_lastMessageInserted->getCommand()->getPlayerID() == msg->getCommand()->getPlayerID()) &&
			isCommandIdNewer(msg->getCommand()->getID(), m_lastMessageInserted->getCommand()->getID()) &&
			((theNext == nullptr) || ((theNext->getCommand()->getNetCommandType() > msg->getCommand()->getNetCommandType()) ||
			 (theNext->getCommand()->getPlayerID() > msg->getCommand()->getPlayerID()) ||
			 isCommandIdNewer(theNext->getCommand()->getID(), msg->getCommand()->getID())))) {

			// Make sure this command isn't already in the list.
			if (isEqualCommandMsg(m_lastMessageInserted->getCommand(), msg->getCommand())) {

				// This command is already in the list, don't duplicate it.
				deleteInstance(msg);
				msg = nullptr;
				return nullptr;
			}

			if (theNext == nullptr) {
				// this means that m_lastMessageInserted == m_last, so m_last should point to the msg that is being inserted.
				msg->setNext(m_lastMessageInserted->getNext());
				msg->setPrev(m_lastMessageInserted);
				m_lastMessageInserted->setNext(msg);
				m_lastMessageInserted = msg;
				m_last = msg;
			} else {
				msg->setNext(m_lastMessageInserted->getNext());
				msg->setPrev(m_lastMessageInserted);
				m_lastMessageInserted->setNext(msg);
				msg->getNext()->setPrev(msg);
				m_lastMessageInserted = msg;
			}
			return msg;
		}
	}

	if (msg->getCommand()->getNetCommandType() > m_last->getCommand()->getNetCommandType()) {
		// easy optimization for a command that goes at the end of the list
		// since they are likely to be added in order.

		// Make sure this command isn't already in the list.
		if (isEqualCommandMsg(m_last->getCommand(), msg->getCommand())) {

			// This command is already in the list, don't duplicate it.
			deleteInstance(msg);
			msg = nullptr;
			return nullptr;
		}

		msg->setPrev(m_last);
		msg->setNext(nullptr);
		m_last->setNext(msg);
		m_last = msg;
		m_lastMessageInserted = msg;
		return msg;
	}

	if (msg->getCommand()->getNetCommandType() < m_first->getCommand()->getNetCommandType()) {
		// Make sure this command isn't already in the list.
		if (isEqualCommandMsg(m_first->getCommand(), msg->getCommand())) {

			// This command is already in the list, don't duplicate it.
			deleteInstance(msg);
			msg = nullptr;
			return nullptr;
		}

		// The command goes at the head of the list.
		msg->setNext(m_first);
		msg->setPrev(nullptr);
		m_first->setPrev(msg);
		m_first = msg;
		m_lastMessageInserted = msg;
		return msg;
	}


	// Find the start of the command type we're looking for.
	NetCommandRef *tempmsg = m_first;
	while ((tempmsg != nullptr) && (msg->getCommand()->getNetCommandType() > tempmsg->getCommand()->getNetCommandType())) {
		tempmsg = tempmsg->getNext();
	}

	if (tempmsg == nullptr) {
		// Make sure this command isn't already in the list.
		if (isEqualCommandMsg(m_last->getCommand(), msg->getCommand())) {

			// This command is already in the list, don't duplicate it.
			deleteInstance(msg);
			msg = nullptr;
			return nullptr;
		}

		// message goes at the end of the list.
		msg->setPrev(m_last);
		msg->setNext(nullptr);
		m_last->setNext(msg);
		m_last = msg;
		m_lastMessageInserted = msg;
		return msg;
	}

	// Now find the player position.  munkee.
	while ((tempmsg != nullptr) && (msg->getCommand()->getNetCommandType() == tempmsg->getCommand()->getNetCommandType()) && (msg->getCommand()->getPlayerID() > tempmsg->getCommand()->getPlayerID())) {
		tempmsg = tempmsg->getNext();
	}

	if (tempmsg == nullptr) {
		// Make sure this command isn't already in the list.
		if (isEqualCommandMsg(m_last->getCommand(), msg->getCommand())) {

			// This command is already in the list, don't duplicate it.
			deleteInstance(msg);
			msg = nullptr;
			return nullptr;
		}

		// message goes at the end of the list.
		msg->setPrev(m_last);
		msg->setNext(nullptr);
		m_last->setNext(msg);
		m_last = msg;
		m_lastMessageInserted = msg;
		return msg;
	}

	// Find the position within the player's section based on the command ID.
	// If the command type doesn't require a command ID, sort by whatever it should be sorted by.
	while (tempmsg != nullptr
		&& msg->getCommand()->getNetCommandType() == tempmsg->getCommand()->getNetCommandType()
		&& msg->getCommand()->getPlayerID() == tempmsg->getCommand()->getPlayerID()
		&& isCommandIdNewer(msg->getCommand()->getSortNumber(), tempmsg->getCommand()->getSortNumber())) {
		tempmsg = tempmsg->getNext();
	}

	if (tempmsg == nullptr) {
		// Make sure this command isn't already in the list.
		if (isEqualCommandMsg(m_last->getCommand(), msg->getCommand())) {

			// This command is already in the list, don't duplicate it.
			deleteInstance(msg);
			msg = nullptr;
			return nullptr;
		}

		// This message goes at the end of the list.
		msg->setPrev(m_last);
		msg->setNext(nullptr);
		m_last->setNext(msg);
		m_last = msg;
		m_lastMessageInserted = msg;
		return msg;
	}

	if (tempmsg == m_first) {
		// Make sure this command isn't already in the list.
		if (isEqualCommandMsg(m_first->getCommand(), msg->getCommand())) {

			// This command is already in the list, don't duplicate it.
			deleteInstance(msg);
			return nullptr;
		}

		// This message goes at the head of the list.
		msg->setNext(m_first);
		msg->setPrev(nullptr);
		m_first->setPrev(msg);
		m_first = msg;
		m_lastMessageInserted = msg;
		return msg;
	}

	// Make sure this command isn't already in the list.
		if (isEqualCommandMsg(tempmsg->getCommand(), msg->getCommand())) {

		// This command is already in the list, don't duplicate it.
		deleteInstance(msg);
		msg = nullptr;
		return nullptr;
	}

	// Insert message before tempmsg.
	msg->setNext(tempmsg);
	msg->setPrev(tempmsg->getPrev());
	msg->getPrev()->setNext(msg);
	tempmsg->setPrev(msg);
	m_lastMessageInserted = msg;

	return msg;
}

Int NetCommandList::length() {
	Int retval = 0;
	NetCommandRef *temp = m_first;
	while (temp != nullptr) {
		++retval;
		temp = temp->getNext();
	}
	return retval;
}

/**
 * This is really inefficient, but we can probably get away with it because
 * there shouldn't be too many messages for any given frame.
 */
NetCommandRef * NetCommandList::findMessage(NetCommandMsg *msg) {
	NetCommandRef *retval = m_first;
	while ((retval != nullptr) && (isEqualCommandMsg(retval->getCommand(), msg) == FALSE)) {
		retval = retval->getNext();
	}
	return retval;
}

NetCommandRef * NetCommandList::findMessage(UnsignedShort commandID, UnsignedByte playerID) {
	NetCommandRef *retval = m_first;
	while (retval != nullptr) {
		if (DoesCommandRequireACommandID(retval->getCommand()->getNetCommandType())) {
			if ((retval->getCommand()->getID() == commandID) && (retval->getCommand()->getPlayerID() == playerID)) {
				return retval;
			}
		}
		retval = retval->getNext();
	}
	return retval;
}

Bool NetCommandList::isEqualCommandMsg(NetCommandMsg *msg1, NetCommandMsg *msg2) {
	if (DoesCommandRequireACommandID(msg1->getNetCommandType()) != DoesCommandRequireACommandID(msg2->getNetCommandType())) {
		return FALSE;
	}

	// At this point we know that the commands both do or do not require a command id.
	// Do or do not, there is no try.
	if (DoesCommandRequireACommandID(msg1->getNetCommandType())) {
		// Are the commands from the same player?
		if (msg1->getPlayerID() != msg2->getPlayerID()) {
			return FALSE;
		}

		// Do they have the same command ID?
		if (msg1->getID() != msg2->getID()) {
			return FALSE;
		}
		return TRUE;
	}

	// If we've gotten this far, we know that the commands do not require a command id.
	// So now our equality checking becomes type-specific.

	// Are they the same type?
	if (msg1->getNetCommandType() != msg2->getNetCommandType()) {
		return FALSE;
	}

	// Are they from the same player?
	if (msg1->getPlayerID() != msg2->getPlayerID()) {
		return FALSE;
	}

	// They are the same type and from the same player.
	// Time for the type specific stuff.
	if (msg1->getNetCommandType() == NETCOMMANDTYPE_ACKSTAGE1) {
		NetAckStage1CommandMsg *ack1 = (NetAckStage1CommandMsg *)msg1;
		NetAckStage1CommandMsg *ack2 = (NetAckStage1CommandMsg *)msg2;

		if (ack1->getOriginalPlayerID() != ack2->getOriginalPlayerID()) {
			return FALSE;
		}

		if (ack1->getCommandID() != ack2->getCommandID()) {
			return FALSE;
		}
		return TRUE;
	}

	// They are the same type and from the same player.
	// Time for the type specific stuff.
	if (msg1->getNetCommandType() == NETCOMMANDTYPE_ACKSTAGE2) {
		NetAckStage2CommandMsg *ack1 = (NetAckStage2CommandMsg *)msg1;
		NetAckStage2CommandMsg *ack2 = (NetAckStage2CommandMsg *)msg2;

		if (ack1->getOriginalPlayerID() != ack2->getOriginalPlayerID()) {
			return FALSE;
		}

		if (ack1->getCommandID() != ack2->getCommandID()) {
			return FALSE;
		}
		return TRUE;
	}

	// They are the same type and from the same player.
	// Time for the type specific stuff.
	if (msg1->getNetCommandType() == NETCOMMANDTYPE_ACKBOTH) {
		NetAckBothCommandMsg *ack1 = (NetAckBothCommandMsg *)msg1;
		NetAckBothCommandMsg *ack2 = (NetAckBothCommandMsg *)msg2;

		if (ack1->getOriginalPlayerID() != ack2->getOriginalPlayerID()) {
			return FALSE;
		}

		if (ack1->getCommandID() != ack2->getCommandID()) {
			return FALSE;
		}
		return TRUE;
	}

	return FALSE;
}
