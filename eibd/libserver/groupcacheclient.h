/*
    EIBD eib bus access and management daemon
    Copyright (C) 2005-2007 Martin K�gler <mkoegler@auto.tuwien.ac.at>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef GROUPCACHECLIENT_H
#define GROUPCACHECLIENT_H

#include "layer3.h"

class ClientConnection;

void CreateGroupCache (Layer3 * l3, Trace * t, bool enable);
void DeleteGroupCache ();

void GroupCacheRequest (Layer3 * l3, Trace * t, ClientConnection * c,
			pth_event_t stop);

#endif