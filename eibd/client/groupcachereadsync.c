/*
    EIBD client library
    Copyright (C) 2005-2007 Martin K�gler <mkoegler@auto.tuwien.ac.at>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    In addition to the permissions in the GNU General Public License, 
    you may link the compiled version of this file into combinations
    with other programs, and distribute those combinations without any 
    restriction coming from the use of this file. (The General Public 
    License restrictions do apply in other respects; for example, they 
    cover modification of the file, and distribution when not linked into 
    a combine executable.)

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "eibclient.h"
#include "eibclient-int.h"

static int
EIB_Cache_Read_Sync_complete (EIBConnection * con)
{
  int i;
  unsigned int j;
  i = _EIB_GetRequest (con);
  if (i == -1)
    return -1;

  if (EIBTYPE (con) != EIB_CACHE_READ)
    {
      errno = ECONNRESET;
      return -1;
    }
  if (!con->buf[4] && !con->buf[5])
    {
      errno = ENODEV;
      return -1;
    }
  if (con->size <= 6)
    {
      errno = ENOENT;
      return -1;
    }
  if (con->req.ptr5)
    *con->req.ptr5 = (con->buf[2] << 8) | con->buf[3];
  j = con->size - 6;
  if (j > con->req.len)
    j = con->req.len;
  memcpy (con->req.buf, con->buf + 6, j);
  return j;
}

int
EIB_Cache_Read_Sync_async (EIBConnection * con, eibaddr_t dst,
			   eibaddr_t * src, int max_len, uint8_t * buf)
{
  uchar head[4];
  int i;
  if (!con)
    {
      errno = EINVAL;
      return -1;
    }
  con->req.len = max_len;
  con->req.buf = buf;
  con->req.ptr5 = src;
  EIBSETTYPE (head, EIB_CACHE_READ);
  EIBSETADDR (head + 2, dst);
  i = _EIB_SendRequest (con, 4, head);
  if (i == -1)
    return -1;
  con->complete = EIB_Cache_Read_Sync_complete;
  return 0;
}


int
EIB_Cache_Read_Sync (EIBConnection * con, eibaddr_t dst,
		     eibaddr_t * src, int max_len, uint8_t * buf)
{
  if (EIB_Cache_Read_Sync_async (con, dst, src, max_len, buf) == -1)
    return -1;
  return EIBComplete (con);
}