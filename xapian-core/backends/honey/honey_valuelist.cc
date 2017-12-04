/** @file honey_valuelist.cc
 * @brief Honey class for value streams.
 */
/* Copyright (C) 2007,2008,2009 Olly Betts
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <config.h>

#include "honey_valuelist.h"

#include "honey_cursor.h"
#include "honey_database.h"
#include "omassert.h"
#include "str.h"

using namespace Honey;
using namespace std;

bool
HoneyValueList::update_reader()
{
    Xapian::docid first_did = docid_from_key(slot, cursor->current_key);
    if (!first_did) return false;

    cursor->read_tag();
    const string & tag = cursor->current_tag;
    reader.assign(tag.data(), tag.size(), first_did);
    return true;
}

HoneyValueList::~HoneyValueList()
{
    delete cursor;
}

Xapian::docid
HoneyValueList::get_docid() const
{
    Assert(!at_end());
    return reader.get_docid();
}

Xapian::valueno
HoneyValueList::get_valueno() const
{
    return slot;
}

std::string
HoneyValueList::get_value() const
{
    Assert(!at_end());
    return reader.get_value();
}

bool
HoneyValueList::at_end() const
{
    return cursor == NULL;
}

void
HoneyValueList::next()
{
    if (!cursor) {
	cursor = db->get_postlist_cursor();
	if (!cursor) return;
	cursor->find_entry_ge(make_valuechunk_key(slot, 1));
    } else if (!reader.at_end()) {
	reader.next();
	if (!reader.at_end()) return;
	cursor->next();
    }

    if (!cursor->after_end()) {
	if (update_reader()) {
	    if (!reader.at_end()) return;
	}
    }

    // We've reached the end.
    delete cursor;
    cursor = NULL;
}

void
HoneyValueList::skip_to(Xapian::docid did)
{
    if (!cursor) {
	cursor = db->get_postlist_cursor();
	if (!cursor) return;
    } else if (!reader.at_end()) {
	reader.skip_to(did);
	if (!reader.at_end()) return;
    }

    if (!cursor->find_entry(make_valuechunk_key(slot, did))) {
	if (update_reader()) {
	    reader.skip_to(did);
	    if (!reader.at_end()) return;
	}
	// The requested docid is between two chunks.
	cursor->next();
    }

    // Either an exact match, or in a gap before the start of a chunk.
    if (!cursor->after_end()) {
	if (update_reader()) {
	    if (!reader.at_end()) return;
	}
    }

    // We've reached the end.
    delete cursor;
    cursor = NULL;
}

bool
HoneyValueList::check(Xapian::docid did)
{
    if (!cursor) {
	cursor = db->get_postlist_cursor();
	if (!cursor) return true;
    } else if (!reader.at_end()) {
	// Check for the requested docid in the current block.
	reader.skip_to(did);
	if (!reader.at_end()) return true;
    }

    // Try moving to the appropriate chunk.
    if (!cursor->find_entry(make_valuechunk_key(slot, did))) {
	// We're in a chunk which might contain the docid.
	if (update_reader()) {
	    reader.skip_to(did);
	    if (!reader.at_end()) return true;
	}
	return false;
    }

    // We had an exact match for a chunk starting with specified docid.
    Assert(!cursor->after_end());
    if (!update_reader()) {
	// We found the exact key we built, so it must match the slot.
	// Therefore update_reader() "can't possibly fail".
	Assert(false);
    }

    return true;
}

string
HoneyValueList::get_description() const
{
    string desc("HoneyValueList(slot=");
    desc += str(slot);
    desc += ')';
    return desc;
}
