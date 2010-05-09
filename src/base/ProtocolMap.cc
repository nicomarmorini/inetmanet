//
// Copyright (C) 2004 Andras Varga
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//


#include <omnetpp.h>
#include "ProtocolMap.h"


void ProtocolMapping::parseProtocolMapping(const char *s)
{
    while (isspace(*s)) s++;

    while (*s)
    {
        Entry entry;

        if (!isdigit(*s))
            throw cRuntimeError("syntax error: protocol number expected");
        entry.protocolNumber = atoi(s);
        while (isdigit(*s)) s++;

        if (*s++!=':')
            throw cRuntimeError("syntax error: colon expected");

        while (isspace(*s)) s++;
        if (!isdigit(*s))
            throw cRuntimeError("syntax error in script: output gate index expected");
        entry.outGateIndex = atoi(s);
        while (isdigit(*s)) s++;

        // add
        entries.push_back(entry);

        // skip delimiter
        while (isspace(*s)) s++;
        if (!*s) break;
        if (*s++!=',')
            throw cRuntimeError("syntax error: comma expected");
        while (isspace(*s)) s++;
    }

}

int ProtocolMapping::getOutputGateForProtocol(int protocol)
{
    for (Entries::iterator i=entries.begin();i!=entries.end();++i)
        if (i->protocolNumber==protocol)
            return i->outGateIndex;
    opp_error("No output gate defined in protocolMapping for protocol number %d", protocol);
    return -1;
}

