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
#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <asm/types.h>
#include <linux/hiddev.h>
#include <sys/ioctl.h>
#include "usbif.h"

USBEndpoint
parseUSBEndpoint (const char *addr)
{
  USBEndpoint e;
  e.bus = -1;
  e.device = -1;
  e.config = -1;
  e.interface = -1;
  if (!*addr)
    return e;
  if (!isdigit (*addr))
    return e;
  e.bus = atoi (addr);
  while (isdigit (*addr))
    addr++;
  if (*addr != ':')
    return e;
  addr++;
  if (!isdigit (*addr))
    return e;
  e.device = atoi (addr);
  while (isdigit (*addr))
    addr++;
  if (*addr != ':')
    return e;
  addr++;
  if (!isdigit (*addr))
    return e;
  e.config = atoi (addr);
  while (isdigit (*addr))
    addr++;
  if (*addr != ':')
    return e;
  addr++;
  if (!isdigit (*addr))
    return e;
  e.interface = atoi (addr);
  return e;
}

void
check_device (usb_device_id_t cdev, USBEndpoint e)
{
  struct usb_device_desc desc;
  struct usb_config_desc cfg;
  struct usb_interface_desc intf;
  struct usb_endpoint_desc ep;
  int in, out, outint;
  usb_dev_handle_t *h;
  int j, k, l;

  if (!cdev)
    return;

  if (usb_get_busnum (usb_get_device_bus_id (cdev)) != e.bus && e.bus != -1)
    return;
  if (usb_get_devnum (cdev) != e.device && e.device != -1)
    return;

  usb_get_device_desc (cdev, &desc);
  for (j = 0; j < desc.bNumConfigurations; j++)
    {
      usb_get_config_desc (cdev, j, &cfg);
      if (cfg.bConfigurationValue != e.config && e.config != -1)
	continue;
      for (k = 0; k < cfg.bNumInterfaces; k++)
	{
	  usb_get_interface_desc (cdev, j, k, &intf);
	  if (intf.bInterfaceNumber != e.interface && e.interface != -1)
	    continue;
	  if (intf.bInterfaceClass != USB_CLASS_HID)
	    continue;

	  in = 0;
	  out = 0;
	  outint = 0;

	  for (l = 0; l < intf.bNumEndpoints; l++)
	    {
	      usb_get_endpoint_desc (cdev, j, k, l, &ep);
	      if (ep.wMaxPacketSize == 64)
		{
		  if (ep.bEndpointAddress & 0x80)
		    {
		      if ((ep.bmAttributes & USB_ENDPOINT_TYPE_MASK) ==
			  USB_ENDPOINT_TYPE_INTERRUPT)
			in = ep.bEndpointAddress;
		    }
		  else
		    {
		      if (((ep.bmAttributes & USB_ENDPOINT_TYPE_MASK) ==
			   USB_ENDPOINT_TYPE_CONTROL && !outint && 0)
			  || (ep.bmAttributes & USB_ENDPOINT_TYPE_MASK) ==
			  USB_ENDPOINT_TYPE_INTERRUPT)
			{
			  out = ep.bEndpointAddress;
			  outint =
			    (ep.bmAttributes & USB_ENDPOINT_TYPE_MASK) ==
			    USB_ENDPOINT_TYPE_INTERRUPT;
			}

		    }
		}
	    }
	  if (!in || !out)
	    continue;
	  if (usb_open (cdev, &h) >= 0)
	    {
	      USBDevice e1;
	      e1.dev = cdev;
	      e1.config = cfg.bConfigurationValue;
	      e1.interface = intf.bInterfaceNumber;
	      e1.sendep = out;
	      e1.recvep = in;
	      usb_close (h);
	      throw e1;
	    }
	}
    }
}


void
check_devlist (usb_device_id_t dev, USBEndpoint e)
{
  usb_device_id_t cdev;
  int i;

  check_device (dev, e);
  for (i = 0; i < usb_get_child_count (dev); i++)
    {
      cdev = usb_get_child_device_id (dev, i + 1);
      check_devlist (cdev, e);
    }

}

USBDevice
detectUSBEndpoint (USBEndpoint e)
{
  usb_bus_id_t bus;
  USBDevice e2;
  try
  {
    for (bus = usb_get_first_bus_id (); bus; bus = usb_get_next_bus_id (bus))
      check_devlist (usb_get_root_device_id (bus), e);
  }
  catch (USBDevice e1)
  {
    return e1;
  }
  e2.dev = -1;
  return e2;
}


USBLowLevelDriver::USBLowLevelDriver (const char *Dev, Trace * tr)
{
  t = tr;
  t->TracePrintf (1, this, "Detect");
  USBEndpoint e = parseUSBEndpoint (Dev);
  d = detectUSBEndpoint (e);
  if (d.dev == -1)
    throw Exception (DEV_OPEN_FAIL);
  t->TracePrintf (1, this, "Using %d (%d:%d:%d:%d) (%d:%d)", d.dev,
		  usb_get_busnum (usb_get_device_bus_id (d.dev)),
		  usb_get_devnum (d.dev), d.config, d.interface, d.sendep,
		  d.recvep);
  if (usb_open (d.dev, &dev) < 0)
    throw Exception (DEV_OPEN_FAIL);
  t->TracePrintf (1, this, "Open");
  if (usb_set_configuration (dev, d.config) < 0)
    throw Exception (DEV_OPEN_FAIL);
  usb_detach_kernel_driver_np (dev, d.interface);
  if (usb_claim_interface (dev, d.interface) < 0)
    throw Exception (DEV_OPEN_FAIL);
  t->TracePrintf (1, this, "Claimed");

  pth_sem_init (&in_signal);
  pth_sem_init (&out_signal);
  pth_sem_init (&send_empty);
  pth_sem_set_value (&send_empty, 1);
  getwait = pth_event (PTH_EVENT_SEM, &out_signal);
  Start ();
  t->TracePrintf (1, this, "Opened");
}

USBLowLevelDriver::~USBLowLevelDriver ()
{
  t->TracePrintf (1, this, "Close");
  Stop ();
  pth_event_free (getwait, PTH_FREE_THIS);

  t->TracePrintf (1, this, "Release");
  usb_release_interface (dev, d.interface);
  usb_attach_kernel_driver_np (dev, d.interface);
  t->TracePrintf (1, this, "Close");
  usb_close (dev);
}


bool
USBLowLevelDriver::Connection_Lost ()
{
  return 0;
}

void
USBLowLevelDriver::Send_Packet (CArray l)
{
  CArray pdu;
  t->TracePacket (1, this, "Send", l);

  inqueue.put (l);
  pth_sem_set_value (&send_empty, 0);
  pth_sem_inc (&in_signal, TRUE);
}

void
USBLowLevelDriver::SendReset ()
{
}

bool
USBLowLevelDriver::Send_Queue_Empty ()
{
  return inqueue.isempty ();
}

pth_sem_t *
USBLowLevelDriver::Send_Queue_Empty_Cond ()
{
  return &send_empty;
}

CArray *
USBLowLevelDriver::Get_Packet (pth_event_t stop)
{
  if (stop != NULL)
    pth_event_concat (getwait, stop, NULL);

  pth_wait (getwait);

  if (stop)
    pth_event_isolate (getwait);

  if (pth_event_status (getwait) == PTH_STATUS_OCCURRED)
    {
      pth_sem_dec (&out_signal);
      CArray *c = outqueue.get ();
      t->TracePacket (1, this, "Recv", *c);
      return c;
    }
  else
    return 0;
}

LowLevelDriverInterface::EMIVer USBLowLevelDriver::getEMIVer ()
{
  return vRaw;
}


void
USBLowLevelDriver::Run (pth_sem_t * stop1)
{
  int i, j;
  pth_event_t stop = pth_event (PTH_EVENT_SEM, stop1);
  pth_event_t input = pth_event (PTH_EVENT_SEM, &in_signal);
  uchar recvbuf[64];
  uchar sendbuf[64];
  usb_io_handle_t *sendh = 0;
  usb_io_handle_t *recvh = 0;
  pth_event_t sende = pth_event (PTH_EVENT_SEM, &in_signal);;
  pth_event_t recve = pth_event (PTH_EVENT_SEM, &in_signal);;

  while (pth_event_status (stop) != PTH_STATUS_OCCURRED)
    {
      if (!recvh)
	{
	  recvh =
	    usb_submit_interrupt_read (dev, d.recvep, recvbuf,
				       sizeof (recvbuf), 1000, 0);
	  if (!recvh)
	    {
	      t->TracePrintf (0, this, "Error StartRecv");
	      break;
	    }
	  t->TracePrintf (0, this, "StartRecv");
	  pth_event (PTH_EVENT_FD | PTH_MODE_REUSE | PTH_UNTIL_FD_READABLE |
		     PTH_UNTIL_FD_WRITEABLE, recve,
		     usb_io_wait_handle (recvh));
	}
      if (recvh)
	pth_event_concat (stop, recve, NULL);
      if (sendh)
	pth_event_concat (stop, sende, NULL);
      else
	pth_event_concat (stop, input, NULL);

      pth_wait (stop);

      pth_event_isolate (sende);
      pth_event_isolate (recve);
      pth_event_isolate (input);

      if (recvh && usb_is_io_completed (recvh))
	{
	  if (usb_io_comp_status (recvh) < 0)
	    t->TracePrintf (0, this, "RecvError %d",
			    usb_io_comp_status (recvh));
	  else
	    {
	      t->TracePrintf (0, this, "RecvComplete %d",
			      usb_io_xfer_size (recvh));
	      CArray res;
	      res.set (recvbuf, sizeof (recvbuf));
	      t->TracePacket (0, this, "RecvUSB", res);
	      outqueue.put (new CArray (res));
	      pth_sem_inc (&out_signal, 1);
	    }
	  usb_io_free (recvh);
	  recvh = 0;
	}

      if (sendh)
	{
	  if (usb_is_io_completed (sendh))
	    {
	      if (usb_io_comp_status (sendh) < 0)
		t->TracePrintf (0, this, "SendError %d",
				usb_io_comp_status (sendh));
	      else
		{
		  t->TracePrintf (0, this, "SendComplete %d",
				  usb_io_xfer_size (sendh));
		  pth_sem_dec (&in_signal);
		  inqueue.get ();
		  if (inqueue.isempty ())
		    pth_sem_set_value (&send_empty, 1);
		}
	      usb_io_free (sendh);
	      sendh = 0;
	    }
	}
      if (!sendh && !inqueue.isempty ())
	{
	  const CArray & c = inqueue.top ();
	  t->TracePacket (0, this, "Send", c);
	  memset (sendbuf, 0, sizeof (sendbuf));
	  memcpy (sendbuf, c.array (),
		  (c () > sizeof (sendbuf) ? sizeof (sendbuf) : c ()));

	  sendh =
	    usb_submit_interrupt_write (dev, d.sendep, sendbuf,
					sizeof (sendbuf), 1000, 0);
	  if (!sendh)
	    {
	      t->TracePrintf (0, this, "Error StartSend");
	      break;
	    }
	  t->TracePrintf (0, this, "StartSend");
	  pth_event (PTH_EVENT_FD | PTH_MODE_REUSE | PTH_UNTIL_FD_READABLE |
		     PTH_UNTIL_FD_WRITEABLE, sende,
		     usb_io_wait_handle (sendh));

	}
    }
  if (sendh)
    {
      usb_io_cancel (sendh);
      usb_io_free (sendh);
    }
  if (recvh)
    {
      usb_io_cancel (recvh);
      usb_io_free (recvh);
    }
  pth_event_free (stop, PTH_FREE_THIS);
  pth_event_free (input, PTH_FREE_THIS);
  pth_event_free (sende, PTH_FREE_THIS);
  pth_event_free (recve, PTH_FREE_THIS);
}