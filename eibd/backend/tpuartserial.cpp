/*
    EIBD eib bus access and management daemon
    Copyright (C) 2005 Martin K�gler <mkoegler@auto.tuwien.ac.at>

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

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "tpuartserial.h"

/** get serial status lines */
static int
getstat (int fd)
{
  int s;
  ioctl (fd, TIOCMGET, &s);
  return s;
}
/** set serial status lines */
static void
setstat (int fd, int s)
{
  ioctl (fd, TIOCMSET, &s);
}


TPUARTSerialLayer2Driver::TPUARTSerialLayer2Driver (const char *dev,
						    eibaddr_t a, Trace * tr)
{
  struct serial_struct snew;
  struct termios t1;
  t = tr;
  t->TracePrintf (2, this, "Open");

  fd = open (dev, O_RDWR | O_NOCTTY | O_NDELAY | O_SYNC);
  if (fd == -1)
    throw Exception (DEV_OPEN_FAIL);
  ioctl (fd, TIOCGSERIAL, &sold);
  ioctl (fd, TIOCGSERIAL, &snew);
  snew.flags |= ASYNC_LOW_LATENCY;
  ioctl (fd, TIOCSSERIAL, &snew);

  close (fd);

  fd = open (dev, O_RDWR | O_NOCTTY | O_SYNC);
  if (fd == -1)
    throw Exception (DEV_OPEN_FAIL);
  tcgetattr (fd, &old);

  t1.c_cflag = B19200 | CS8 | CLOCAL | CREAD | PARENB;
  t1.c_iflag = IGNBRK | INPCK | ISIG;
  t1.c_oflag = 0;
  t1.c_lflag = 0;
  t1.c_cc[VTIME] = 1;
  t1.c_cc[VMIN] = 0;

  tcsetattr (fd, TCSAFLUSH, &t1);

  setstat (fd, (getstat (fd) & ~TIOCM_RTS) | TIOCM_DTR);

  mode = 0;
  vmode = 0;
  addr = a;
  indaddr.resize (1);
  indaddr[0] = a;

  pth_sem_init (&in_signal);
  pth_sem_init (&out_signal);

  getwait = pth_event (PTH_EVENT_SEM, &out_signal);

  Start ();
  t->TracePrintf (2, this, "Openend");
}

TPUARTSerialLayer2Driver::~TPUARTSerialLayer2Driver ()
{
  t->TracePrintf (2, this, "Close");
  Stop ();
  pth_event_free (getwait, PTH_FREE_THIS);

  while (!outqueue.isempty ())
    delete outqueue.get ();
  while (!inqueue.isempty ())
    delete inqueue.get ();

  if (fd != -1)
    {
      setstat (fd, (getstat (fd) & ~TIOCM_RTS) & ~TIOCM_DTR);
      tcsetattr (fd, TCSAFLUSH, &old);
      ioctl (fd, TIOCSSERIAL, &sold);
      close (fd);
    }

}

bool
TPUARTSerialLayer2Driver::addAddress (eibaddr_t addr)
{
  unsigned i;
  for (i = 0; i < indaddr (); i++)
    if (indaddr[i] == addr)
      return 0;
  indaddr.resize (indaddr () + 1);
  indaddr[indaddr () - 1] = addr;
  return 1;
}

bool
TPUARTSerialLayer2Driver::addGroupAddress (eibaddr_t addr)
{
  unsigned i;
  for (i = 0; i < groupaddr (); i++)
    if (groupaddr[i] == addr)
      return 0;
  groupaddr.resize (groupaddr () + 1);
  groupaddr[groupaddr () - 1] = addr;
  return 1;
}

bool
TPUARTSerialLayer2Driver::removeAddress (eibaddr_t addr)
{
  unsigned i;
  for (i = 0; i < indaddr (); i++)
    if (indaddr[i] == addr)
      {
	indaddr.deletepart (i, 1);
	return 1;
      }
  return 0;
}

bool
TPUARTSerialLayer2Driver::removeGroupAddress (eibaddr_t addr)
{
  unsigned i;
  for (i = 0; i < groupaddr (); i++)
    if (groupaddr[i] == addr)
      {
	groupaddr.deletepart (i, 1);
	return 1;
      }
  return 0;
}

bool TPUARTSerialLayer2Driver::openVBusmonitor ()
{
  vmode = 1;
  return 1;
}

bool TPUARTSerialLayer2Driver::closeVBusmonitor ()
{
  vmode = 0;
  return 1;
}

eibaddr_t
TPUARTSerialLayer2Driver::getDefaultAddr ()
{
  return addr;
}

bool
TPUARTSerialLayer2Driver::Connection_Lost ()
{
  return 0;
}

bool
TPUARTSerialLayer2Driver::Send_Queue_Empty ()
{
  return inqueue.isempty ();
}

void
TPUARTSerialLayer2Driver::Send_L_Data (LPDU * l)
{
  t->TracePrintf (2, this, "Send %s", l->Decode ()());
  inqueue.put (l);
  pth_sem_inc (&in_signal, 1);
}

LPDU *
TPUARTSerialLayer2Driver::Get_L_Data (pth_event_t stop)
{
  if (stop != NULL)
    pth_event_concat (getwait, stop, NULL);

  pth_wait (getwait);

  if (stop)
    pth_event_isolate (getwait);

  if (pth_event_status (getwait) == PTH_STATUS_OCCURRED)
    {
      pth_sem_dec (&out_signal);
      LPDU *l = outqueue.get ();
      t->TracePrintf (2, this, "Recv %s", l->Decode ()());
      return l;
    }
  else
    return 0;
}


//Open

bool
TPUARTSerialLayer2Driver::enterBusmonitor ()
{
  uchar c = 0x05;
  t->TracePacket (2, this, "openBusmonitor", 1, &c);
  write (fd, &c, 1);
  mode = 1;
  return 1;
}

bool
TPUARTSerialLayer2Driver::leaveBusmonitor ()
{
  uchar c = 0x01;
  t->TracePacket (2, this, "leaveBusmonitor", 1, &c);
  write (fd, &c, 1);
  mode = 0;
  return 1;
}

bool
TPUARTSerialLayer2Driver::Open ()
{
  uchar c = 0x01;
  t->TracePacket (2, this, "open-reset", 1, &c);
  write (fd, &c, 1);
  return 1;
}

bool
TPUARTSerialLayer2Driver::Close ()
{
  return 1;
}

void
TPUARTSerialLayer2Driver::RecvLPDU (const uchar * data, int len)
{
  t->TracePacket (1, this, "Recv", len, data);
  if (mode || vmode)
    {
      L_Busmonitor_PDU *l = new L_Busmonitor_PDU;
      l->pdu.set (data, len);
      outqueue.put (l);
      pth_sem_inc (&out_signal, 1);
    }
  if (!mode)
    {
      LPDU *l = LPDU::fromPacket (CArray (data, len));
      if(l->getType()==L_Data&&!((L_Data_PDU*)l)->repeated&&((L_Data_PDU*)l)->valid_checksum)
	{
	  outqueue.put (l);
	  pth_sem_inc (&out_signal, 1);
	}
      else
	delete l;
    }
}

void
TPUARTSerialLayer2Driver::Run (pth_sem_t * stop1)
{
  uchar buf[255];
  int i;
  CArray in;
  int to = 0;
  int waitconfirm = 0;
  int acked = 0;
  pth_event_t stop = pth_event (PTH_EVENT_SEM, stop1);
  pth_event_t input = pth_event (PTH_EVENT_SEM, &in_signal);
  pth_event_t timeout = pth_event (PTH_EVENT_TIME, pth_timeout (0, 0));
  while (pth_event_status (stop) != PTH_STATUS_OCCURRED)
    {
      if (in () == 0 && !waitconfirm)
	pth_event_concat (stop, input, NULL);
      if (to)
	pth_event_concat (stop, timeout, NULL);
      i = pth_read_ev (fd, buf, sizeof (buf), stop);
      pth_event_isolate (stop);
      pth_event_isolate (timeout);
      if (i > 0)
	{
	  t->TracePacket (0, this, "Recv", i, buf);
	  in.setpart (buf, in (), i);
	}
      while (in () > 0)
	{
	  if (in[0] == 0x8B)
	    {
	      if (waitconfirm)
		{
		  waitconfirm = 0;
		  delete inqueue.get ();
		  pth_sem_dec (&in_signal);
		}
	      in.deletepart (0, 1);
	    }
	  else if (in[0] == 0xCC || in[0] == 0xC0 || in[0] == 0x0C)
	    {
	      RecvLPDU (in.array (), 1);
	      in.deletepart (0, 1);
	    }
	  else if ((in[0] & 0x80) == 0x80)
	    {
	      if (in () < 6)
		{
		  if (!to)
		    {
		      to = 1;
		      pth_event (PTH_EVENT_TIME | PTH_MODE_REUSE, timeout,
				 pth_timeout (0, 300000));
		    }
		  if (pth_event_status (timeout) != PTH_STATUS_OCCURRED)
		    break;
		  t->TracePrintf (0, this, "Remove1 %02X", in[0]);
		  in.deletepart (0, 1);
		  continue;
		}
	      if (!acked)
		{
		  uchar c = 0x10;
		  if ((in[5] & 0x80) == 0)
		    {
		      for (unsigned i = 0; i < indaddr (); i++)
			if (indaddr[i] == (in[3] << 8) | in[4])
			  c |= 0x1;
		    }
		  else
		    {
		      for (unsigned i = 0; i < groupaddr (); i++)
			if (groupaddr[i] == (in[3] << 8) | in[4])
			  c |= 0x1;
		    }
		  t->TracePrintf (0, this, "SendAck %02X", c);
		  write (fd, &c, 1);
		  acked = 1;
		}
	      unsigned len = in[5] & 0x0f;
	      len += 6 + 2;
	      if (in () < len)
		{
		  if (!to)
		    {
		      to = 1;
		      pth_event (PTH_EVENT_TIME | PTH_MODE_REUSE, timeout,
				 pth_timeout (0, 300000));
		    }
		  if (pth_event_status (timeout) != PTH_STATUS_OCCURRED)
		    break;
		  t->TracePrintf (0, this, "Remove2 %02X", in[0]);
		  in.deletepart (0, 1);
		  continue;
		}
	      acked = 0;
	      RecvLPDU (in.array (), len);
	      in.deletepart (0, len);
	    }
	  else
	    {
	      acked = 0;
	      t->TracePrintf (0, this, "Remove %02X", in[0]);
	      in.deletepart (0, 1);
	    }
	  to = 0;
	}
      if (in () == 0 && !inqueue.isempty () && !waitconfirm)
	{
	  LPDU *l = (LPDU *) inqueue.top ();
	  CArray d = l->ToPacket ();
	  CArray w;
	  unsigned i;
	  int j;
	  w.resize (d () * 2);
	  for (i = 0; i < d (); i++)
	    {
	      w[2 * i] = 0x80 | (i & 0x0f);
	      w[2 * i + 1] = d[i];
	    }
	  w[(d () * 2) - 2] = (w[(d () * 2) - 2] & 0x0f) | 0x40;
	  t->TracePacket (0, this, "Write", w);
	  j = pth_write_ev (fd, w.array (), w (), stop);
	  waitconfirm = 1;
	}
    }
  pth_event_free (stop, PTH_FREE_THIS);
  pth_event_free (input, PTH_FREE_THIS);
  pth_event_free (timeout, PTH_FREE_THIS);
}