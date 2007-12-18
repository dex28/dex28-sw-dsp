/* $Id: zap_xhfc_su.c,v 1.5 2006/02/15 15:06:52 martinb1 Exp $
 *
 * Zaptel driver for CologneChip AG's XHFC
 *
 * Authors : Martin Bachem, Joerg Ciesielski
 * Contact : info@colognechip.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *******************************************************************************
 *
 * MODULE PARAMETERS:
 *
 * - protocol=<p1>[,p2,p3...]
 *   Values:
 *        <bit 0>   0x01  Net side stack (NT mode)
 *        <bit 1>   0x02  Line Interface Mode (0=S0, 1=Up)
 *        <bit 2>   0x04  st line polarity (1=exchanged)
 * - debug:
 *        enable debugging (see xhfc_su.h for debug options)
 *
 */
 
#include <linux/config.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <asm/timex.h>
#include <zaptel.h>
#include "zap_xhfc_su.h"
#include "xhfc24succ.h"

#if BRIDGE == BRIDGE_PCI2PI
#include "xhfc_pci2pi.h"
#endif

static const char xhfc_rev[] = "$Revision: 1.5 $";

#define MAX_CARDS	8
static int card_cnt;
static u_int protocol[MAX_CARDS * MAX_PORT];

#ifdef MODULE
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
#define MODULE_PARM_T	"1-8i"
MODULE_PARM(debug, "1i");
MODULE_PARM(protocol, MODULE_PARM_T);
#endif

static int debug = 0;

/* static function prototypes */
static void setup_su(xhfc_hw * hw, __u8 pt, __u8 bc, __u8 enable);
static void init_su(xhfc_hw * hw, __u8 pt);
static void enable_interrupts(xhfc_hw * hw);
static void xhfc_ph_command(xhfc_port_t * port, u_char command);
static void setup_fifo(xhfc_hw * hw, __u8 fifo, __u8 conhdlc, __u8 subcfg,
	__u8 fifoctrl, __u8 enable);


static int zthfc_spanconfig(struct zt_span *span,struct zt_lineconfig *lc) {
	printk(KERN_INFO "%s %s\n", __FUNCTION__, span->name);
	return 0;
}


static int
zthfc_chanconfig(struct zt_chan *chan, int sigtype) {
	printk(KERN_INFO "%s %s\n", __FUNCTION__, chan->name);
	return 0;
}

static int
zthfc_maint(struct zt_span *span, int cmd) {
	printk(KERN_INFO "%s %s \n", __FUNCTION__, span->name);
	return 0;
}

static int
zthfc_close(struct zt_chan *chan) {
	printk(KERN_INFO "%s %s\n", __FUNCTION__, chan->name);
	module_put(THIS_MODULE);
	return 0;
}

static int
zthfc_open(struct zt_chan *chan) {
	xhfc_port_t * port = chan->pvt;
	unsigned long flags;
	
	printk(KERN_INFO "%s %s\n", __FUNCTION__, chan->name);
	try_module_get(THIS_MODULE);

	spin_lock_irqsave(&port->hw->lock, flags);
	
	/* try to activate when L1 port channel gets opened,
	   e.g. at asterisk startup */
	if ((!(test_bit(HFC_L1_ACTIVATED, &port->l1_flags))) &&
	    (!(test_bit(HFC_L1_ACTIVATING, &port->l1_flags)))) {
		// e.g. reset zaptel alarms
		if (port->zspan.alarms != ZT_ALARM_NONE) {
			port->zspan.alarms = ZT_ALARM_NONE;
			zt_alarm_notify(&port->zspan);
		}

	    	xhfc_ph_command(port, (port->mode & PORT_MODE_TE)?
	    	                      HFC_L1_ACTIVATE_TE:HFC_L1_ACTIVATE_NT);
	}
	
	spin_unlock_irqrestore(&port->hw->lock, flags);

	return 0;
}

static int
zthfc_ioctl(struct zt_chan *chan, unsigned int cmd, unsigned long data) {
	xhfc_port_t * port = (xhfc_port_t *)chan->pvt;
	
	printk(KERN_INFO "%s cmd(%i) port(%i) chan(%i)\n", __FUNCTION__, cmd, port->idx, chan->chanpos);
        switch(cmd) {
        	default:
                	return -ENOTTY;
        }
        return 0;
}

static int
zthfc_rbsbits(struct zt_chan *chan, int bits) {
	printk(KERN_INFO "%s\n", __FUNCTION__);
	return 0;
}

static int
zthfc_shutdown(struct zt_span *span) {
	xhfc_port_t * port = (xhfc_port_t *)span->pvt;
	__u8 bc;
	unsigned long flags;
		
	printk(KERN_INFO "%s %s\n", __FUNCTION__, span->name);
	
	if (!(span->flags & ZT_FLAG_RUNNING))
		return(0);
	
	spin_lock_irqsave(&port->hw->lock, flags);

	/* disable B-Channel Fifos */
	for (bc=0; bc<2; bc++) {	
		setup_fifo(port->hw, (port->idx * 8)+(bc*2),   6, 0, 0, 0);
		setup_fifo(port->hw, (port->idx * 8)+(bc*2)+1, 6, 0, 0, 0);
		setup_su(port->hw, port->idx, bc, 0);
	}

	if (port->mode & PORT_MODE_NT)
		xhfc_ph_command(port, HFC_L1_DEACTIVATE_NT);

	spin_unlock_irqrestore(&port->hw->lock, flags);	
	return 0;
}

static int
zthfc_startup(struct zt_span *span) {
	xhfc_port_t * port = (xhfc_port_t *)span->pvt;
	__u8 bc;
	unsigned long flags;

	if (debug & DEBUG_HFC_INIT) 
		printk (KERN_INFO "%s %s\n", __FUNCTION__, span->name);

	spin_lock_irqsave(&port->hw->lock, flags);
	
	for (bc=0; bc<2; bc++) {
		/* setup B1 channel buffers (8 bytes each) */
		memset(port->brxbuf[bc], 0x0, sizeof(port->brxbuf[bc]));
		port->zchans[bc].readchunk = port->brxbuf[bc];
		memset(port->btxbuf[bc], 0x0, sizeof(port->btxbuf[bc]));
		port->zchans[bc].writechunk = port->btxbuf[bc];
		
		setup_fifo(port->hw, (port->idx * 8)+(bc*2),   6, 0, 0, 1);	// enable TX Fifo
		setup_fifo(port->hw, (port->idx * 8)+(bc*2)+1, 6, 0, 0, 1);	// enable RX Fifo
		setup_su(port->hw, port->idx, bc, 1);
	}

	/* setup D channel buffers */
	memset(port->drxbuf, 0x0, sizeof(port->drxbuf));
	port->zchans[2].readchunk = port->drxbuf;
	memset(port->dtxbuf, 0x0, sizeof(port->dtxbuf));
	port->zchans[2].writechunk = port->dtxbuf;
	port->zchans[2].maxbytes2transmit = MAX_DFRAME_LEN_L1;

	/* zaptel chan 2 used as HDLC D-Channel */
	setup_fifo(port->hw, (port->idx * 8)+4, 5, 2, M_FR_ABO*0, 1); /* D TX */
	setup_fifo(port->hw, (port->idx * 8)+5, 5, 2, M_FR_ABO*0, 1); /* D RX */
	port->zchans[2].flags &= ~ZT_FLAG_HDLC;
	port->zchans[2].flags |= ZT_FLAG_BRIDCHAN;

	if (debug & DEBUG_HFC_INIT) 
		printk (KERN_INFO "%s %s starting port(%i)\n", __FUNCTION__, span->name,
		        port->idx);

	/* enable this port's state machine */
	write_xhfc(port->hw, R_SU_SEL, port->idx);
	write_xhfc(port->hw, A_SU_WR_STA, 0);

	if (port->mode & PORT_MODE_TE)
		xhfc_ph_command(port, HFC_L1_ACTIVATE_TE);
	else
		xhfc_ph_command(port, HFC_L1_ACTIVATE_NT);

	span->flags |= ZT_FLAG_RUNNING;

	spin_unlock_irqrestore(&port->hw->lock, flags);	
	return(0);
}

static int
init_zaptel_interface(xhfc_port_t * port)
{
	__u8 i;
	
	port->zspan.spanconfig = zthfc_spanconfig;
	port->zspan.chanconfig = zthfc_chanconfig;
	port->zspan.startup = zthfc_startup;
	port->zspan.shutdown = zthfc_shutdown;
	port->zspan.maint = zthfc_maint;
	port->zspan.rbsbits = zthfc_rbsbits;
	port->zspan.open = zthfc_open;
	port->zspan.close = zthfc_close;
	port->zspan.ioctl = zthfc_ioctl;

	port->zspan.chans = port->zchans;
	port->zspan.channels = 3;
	port->zspan.deflaw = ZT_LAW_ALAW;
	port->zspan.linecompat = ZT_CONFIG_AMI | ZT_CONFIG_CCS; // <--- this is really BS
	init_waitqueue_head(&port->zspan.maintq);
	port->zspan.pvt = port;
	port->zspan.offset = port->idx;
	
	sprintf(port->zspan.name, "%s%d/%d", DRIVER_NAME, port->hw->cardnum, port->idx);

	for (i=0; i < port->zspan.channels; i++) {
		memset(&(port->zchans[i]),0x0,sizeof(struct zt_chan));
		sprintf(port->zchans[i].name, "%s/%d", port->zspan.name, i + 1);
		port->zchans[i].pvt = port;
		port->zchans[i].sigcap =  ZT_SIG_EM | ZT_SIG_CLEAR | ZT_SIG_FXSLS |
		                          ZT_SIG_FXSGS | ZT_SIG_FXSKS | ZT_SIG_FXOLS |
		                          ZT_SIG_FXOGS | ZT_SIG_FXOKS | ZT_SIG_CAS |
		                          ZT_SIG_SF;
		port->zchans[i].chanpos = i + 1; 
	}

	if (zt_register(&port->zspan,0)) {
		printk(KERN_INFO DRIVER_NAME ": unable to register zaptel span %d!\n",port->idx+1);
		return -1;
	}
	
	return(0);
}
	

static inline void
xhfc_waitbusy(xhfc_hw * hw)
{
	while (read_xhfc(hw, R_STATUS) & M_BUSY);
}

static inline void
xhfc_selfifo(xhfc_hw * hw, __u8 fifo)
{
	write_xhfc(hw, R_FIFO, fifo);
	xhfc_waitbusy(hw);
}

static inline void
xhfc_inc_f(xhfc_hw * hw)
{
	write_xhfc(hw, A_INC_RES_FIFO, M_INC_F);
	xhfc_waitbusy(hw);
}

static inline void
xhfc_resetfifo(xhfc_hw * hw)
{
	write_xhfc(hw, A_INC_RES_FIFO, M_RES_FIFO | M_RES_FIFO_ERR);
	xhfc_waitbusy(hw);
}


/****************************************************/
/* Physical S/U commands to control Line Interface  */
/****************************************************/
static void
xhfc_ph_command(xhfc_port_t * port, u_char command)
{
	xhfc_hw *hw = port->hw;

	switch (command) {
		case HFC_L1_ACTIVATE_TE:
			if (debug & DEBUG_HFC_S0_STATES)
				printk (KERN_INFO "HFC_L1_ACTIVATE_TE port(%i)\n",
				        port->idx);

			set_bit(HFC_L1_ACTIVATING, &port->l1_flags);
			write_xhfc(hw, R_SU_SEL, port->idx);
			write_xhfc(hw, A_SU_WR_STA, STA_ACTIVATE);
			port->t3 = HFC_TIMER_T3;
			break;

		case HFC_L1_FORCE_DEACTIVATE_TE:
			if (debug & DEBUG_HFC_S0_STATES)
				printk (KERN_INFO "HFC_L1_FORCE_DEACTIVATE_TE port(%i)\n",
				        port->idx);
				        
			write_xhfc(hw, R_SU_SEL, port->idx);
			write_xhfc(hw, A_SU_WR_STA, STA_DEACTIVATE);
			break;

		case HFC_L1_ACTIVATE_NT:
			if (debug & DEBUG_HFC_S0_STATES)
				printk (KERN_INFO "HFC_L1_ACTIVATE_NT port(%i)\n",
				        port->idx);

			set_bit(HFC_L1_ACTIVATING, &port->l1_flags);
			write_xhfc(hw, R_SU_SEL, port->idx);
			write_xhfc(hw, A_SU_WR_STA,
				   STA_ACTIVATE | M_SU_SET_G2_G3);
			break;

		case HFC_L1_DEACTIVATE_NT:
			if (debug & DEBUG_HFC_S0_STATES)
				printk (KERN_INFO "HFC_L1_DEACTIVATE_NT port(%i)\n",
				        port->idx);

			write_xhfc(hw, R_SU_SEL, port->idx);
			write_xhfc(hw, A_SU_WR_STA, STA_DEACTIVATE);
			break;
	}
}


static void
su_new_state(xhfc_port_t * port, __u8 su_state)
{
	reg_a_su_rd_sta new_state;

	new_state.reg = su_state;

	if (port->su_state.bit.v_su_sta != new_state.bit.v_su_sta) {
		if (debug & DEBUG_HFC_S0_STATES) 
			printk(KERN_INFO "%s %s %s%i\n", __FUNCTION__,
			       port->zspan.name,
			       (port->mode & PORT_MODE_NT)?"G":"F",
			       new_state.bit.v_su_sta);

		if (port->mode & PORT_MODE_TE) {
	
			/* disable T3 ? */
			if ((new_state.bit.v_su_sta <= 3) || (new_state.bit.v_su_sta >= 7))
				port->t3 = -1; 
	
			switch (new_state.bit.v_su_sta) {
				case (3):
					if (test_and_clear_bit(HFC_L1_ACTIVATED, &port->l1_flags))
						port->t4 = HFC_TIMER_T4;
					break;

				case (7):
					if (port->t4 > HFC_TIMER_OFF)
						port->t4 = HFC_TIMER_OFF;
						
					clear_bit(HFC_L1_ACTIVATING, &port->l1_flags);
					set_bit(HFC_L1_ACTIVATED, &port->l1_flags);
					
					port->zspan.alarms = ZT_ALARM_NONE;
					zt_alarm_notify(&port->zspan);
					break;

				case (8):
					port->t4 = HFC_TIMER_OFF;
					break;
				default:
					break;
			}

		} else if (port->mode & PORT_MODE_NT) {

			switch (new_state.bit.v_su_sta) {
				case (1):
					clear_bit(HFC_L1_ACTIVATED, &port->l1_flags);
					port->nt_timer = 0;
					port->mode &= ~NT_TIMER;
					break;
				case (2):
					if (port->nt_timer < 0) {
						port->nt_timer = 0;
						port->mode &= ~NT_TIMER;
						clear_bit(HFC_L1_ACTIVATING, &port->l1_flags);
						
						xhfc_ph_command(port, HFC_L1_DEACTIVATE_NT);
					} else {
						port->nt_timer = NT_T1_COUNT;
						port->mode |= NT_TIMER;

						write_xhfc(port->hw, R_SU_SEL, port->idx);
						write_xhfc(port->hw, A_SU_WR_STA, M_SU_SET_G2_G3);
					}
					break;
				case (3):
					clear_bit(HFC_L1_ACTIVATING, &port->l1_flags);
					set_bit(HFC_L1_ACTIVATED, &port->l1_flags);
					port->nt_timer = 0;
					port->mode &= ~NT_TIMER;
					break;
				case (4):
					port->nt_timer = 0;
					port->mode &= ~NT_TIMER;
					break;
				default:
					break;
			}
		}
		port->su_state.bit.v_su_sta = new_state.bit.v_su_sta;
	}
}

static void
xhfc_read_fifo_dchan(xhfc_port_t * port)
{
	xhfc_hw * hw = port->hw;
	__u8	*buf = port->drxbuf;
	int	*idx = &port->drx_indx;
	__u8	*data; /* pointer for new data */
	__u8	rcnt, i;
	__u8	f1=0, f2=0, z1=0, z2=0;

	/* select D-RX fifo */
	xhfc_selfifo(hw, (port->idx * 8) + 5);

	/* hdlc rcnt */
	f1 = read_xhfc(hw, A_F1);
	f2 = read_xhfc(hw, A_F2);
	z1 = read_xhfc(hw, A_Z1);
	z2 = read_xhfc(hw, A_Z2);

	rcnt = (z1 - z2) & hw->max_z;
	if (f1 != f2)
		rcnt++;	

	if (rcnt > 0) {
		data = buf + *idx;
		*idx += rcnt;

		/* read data from FIFO */
		i=0;
		while (i<rcnt) {
			if ((rcnt-i) >= 4) {
				*((__u32 *) (data+i)) = read32_xhfc(port->hw, A_FIFO_DATA);
				i += 4;
			} else {
				*(data+i) = read_xhfc(port->hw, A_FIFO_DATA);
				i++;
			}
		}

		/* hdlc frame termination */	
		if (f1 != f2) {
			xhfc_inc_f(hw);

			/* check minimum frame size */
			if (*idx < 4) {
				if (debug & DEBUG_HFC_FIFO_ERR)
					printk (KERN_INFO "%s: frame in port(%i) < minimum size\n",
							  __FUNCTION__,
							  port->idx);
				goto read_exit;
			}

			/* check crc */
			if (buf[(*idx) - 1]) {
				if (debug & DEBUG_HFC_FIFO_ERR)
					printk (KERN_INFO "%s: port(%i) CRC-error\n",
							  __FUNCTION__,
							  port->idx);
				goto read_exit;	
			}

			/* D-Channel debug to syslog */
			if (debug & DEBUG_HFC_DTRACE) {
				printk (KERN_INFO "%s D-RX len(%02i): ",
				        port->zspan.name,
				        (*idx));
				i = 0;
				while (i < (*idx)) {
					printk("%02x ",
					       buf[i++]);
					if (i == (*idx - 3))
						printk ("- ");
				}
				printk("\n");
			}

			port->zchans[2].bytes2receive = *idx-1;
			port->zchans[2].eofrx = 1;			
read_exit:
			*idx = 0;
		}
	}
}

static void
xhfc_write_fifo_dchan(xhfc_port_t * port)
{
	xhfc_hw * hw = port->hw;
	__u8	*buf = NULL;
	int 	*len = NULL,
		*idx = NULL;
	__u8	fcnt, tcnt, i;
	__u8	free;
	__u8	f1, f2;
	__u8	fstat;
	__u8	*data;	

	buf = port->dtxbuf;			/* data buffer */
	len = &port->zchans[2].bytes2transmit;	/* hdlc package len */
	idx = &port->dtx_indx;			/* already transmitted */

	xhfc_selfifo(hw, (port->idx*8+4));

	fstat = read_xhfc(hw, A_FIFO_STA);
	free = (hw->max_z - (read_xhfc(hw, A_USAGE)));
	tcnt = ((free >= (*len - *idx)) ? (*len - *idx) : free);

	f1 = read_xhfc(hw, A_F1);
	f2 = read_xhfc(hw, A_F2);
	fcnt = 0x07 - ((f1 - f2) & 0x07);	/* free frame count in tx fifo */

	if (free && fcnt && tcnt) {
		data = buf + *idx;
		*idx += tcnt;

		/* write data to FIFO */
		i=0;
		while (i<tcnt) {
			if ((tcnt-i) >= 4) {
				write32_xhfc(port->hw, A_FIFO_DATA, *((__u32 *) (data+i)));
				i += 4;
			} else {
				write_xhfc(port->hw, A_FIFO_DATA, *(data+i));
				i++;
			}
		}
			
		/* terminate frame */
		if (*idx == *len) {

			/* D-Channel debug to syslog */
			if (debug & DEBUG_HFC_DTRACE) {
				printk (KERN_INFO "%s D-TX len(%02i): ",
				        port->zspan.name,
				        (*idx));
				i = 0;
				while (i < (*idx))
					printk("%02x ",
					       buf[i++]);
				printk("\n");
			}

			xhfc_inc_f(hw);
			*len = 0;
			*idx = 0;
		}
	}
}

/* write 8 bytes transparent audio data to Fifo */
static void
xhfc_write_fifo_bchan(xhfc_port_t * port, __u8 bc) {
	__u8 i, free;

	xhfc_selfifo(port->hw, (port->idx*8+(bc*2)+M_REV));
	
	free = (port->hw->max_z - (read_xhfc(port->hw, A_USAGE)));
	if (free < ZT_CHUNKSIZE) {
		printk (KERN_INFO "%s: CRITICAL FIFO OVERRUN bc(%d)\n",
		        port->zspan.name, bc);
		return;
	}

	i=0;
	while (i<ZT_CHUNKSIZE) {
		if ((ZT_CHUNKSIZE-i) >= 4) {
			write32_xhfc(port->hw, A_FIFO_DATA, *((__u32 *) &port->btxbuf[bc][i]));
			i += 4;
		} else {
			write_xhfc(port->hw, A_FIFO_DATA, port->btxbuf[bc][i]);
			i++;
		}
	}
}

/* read 8 bytes transparent audio data from Fifo */
static void
xhfc_read_fifo_bchan(xhfc_port_t * port, __u8 bc) {
	__u8 i=0;
	int mumbojumbo=0;
	__u8 rcnt;

	xhfc_selfifo(port->hw, (port->idx*8+(bc*2+1))+M_REV);
	rcnt =  read_xhfc(port->hw, A_USAGE) - 1;

	/* not enough data in fifo buffer */
	if (rcnt < ZT_CHUNKSIZE) {
		printk (KERN_INFO "%s: FIFO FILL LEVEL TOO LOW - bc(%d)\n",
		        port->zspan.name, bc);
		memset(port->brxbuf[bc], 0xFF, ZT_CHUNKSIZE);
		return;
	}

	/* too much data in fifo, so reduce it to 2*ZT_CHUNKSIZE */
	if (rcnt > 2*ZT_CHUNKSIZE) {
		printk (KERN_INFO "%s: FIFO FILL LEVEL TOO HIGH -  bc(%d) rcnt(%d)\n",
		        port->zspan.name, bc, rcnt);

		mumbojumbo = rcnt - 2*ZT_CHUNKSIZE; 
		rcnt -= mumbojumbo;
		while (mumbojumbo--)
			read_xhfc(port->hw, A_FIFO_DATA);
		xhfc_selfifo(port->hw, (port->idx*8+(bc*2+1))+M_REV);
	}

	i=0;
	while (i<ZT_CHUNKSIZE) {
		if ((ZT_CHUNKSIZE-i) >= 4) {
			*((__u32 *) &port->brxbuf[bc][i]) = read32_xhfc(port->hw, A_FIFO_DATA);
			i += 4;
		} else {
			port->brxbuf[bc][i] = read_xhfc(port->hw, A_FIFO_DATA);
			i++;
		}
	}
	
	xhfc_selfifo(port->hw, (port->idx*8+(bc*2+1))+M_REV);
	zt_ec_chunk(&port->zchans[bc], port->zchans[bc].readchunk, port->zchans[bc].writechunk);
}


static void
xhfc_irq_work(xhfc_hw * hw)
{
	__u8 pt, bc;

	hw->su_irq.reg |= read_xhfc(hw, R_SU_IRQ);

	/* get fifo IRQ states in bundle */
	for (pt = 0; pt < hw->num_ports; pt++) {
		hw->fifo_irq |=
		    (read_xhfc(hw, R_FIFO_BL0_IRQ + pt) << (pt* 8));
	}
	
	for (pt = 0; pt < hw->num_ports; pt++) {
		if (hw->port[pt].zspan.flags & ZT_FLAG_RUNNING) {

			/* handle S/U statechage */
			if (hw->su_irq.reg & (1 << pt)) {
				write_xhfc(hw, R_SU_SEL, pt);
				su_new_state(&hw->port[pt], read_xhfc(hw, A_SU_RD_STA));
				hw->su_irq.reg &= ~(1 << pt);
			}

			/* get tx buffer filled by zaptel */
			zt_transmit(&hw->port[pt].zspan); 

			/* transmit D-Channel Data */
			if (hw->port[pt].zchans[2].bytes2transmit) {
				if (test_bit(HFC_L1_ACTIVATED, &hw->port[pt].l1_flags)) {
					xhfc_write_fifo_dchan(&hw->port[pt]);
				} else {
					if (!(test_bit(HFC_L1_ACTIVATING, &hw->port[pt].l1_flags))) {
						if (hw->port[pt].mode & PORT_MODE_TE)
							xhfc_ph_command(&hw->port[pt],
							                HFC_L1_ACTIVATE_TE);
						else
							xhfc_ph_command(&hw->port[pt],
							                HFC_L1_ACTIVATE_NT);
					}
				}
			}
			
			/* transmit B-Channel Data */
			for (bc=0; bc<2; bc++)
				xhfc_write_fifo_bchan(&hw->port[pt], bc);

			/* set bit in fifo_irq when RX data over treshold */
			hw->fifo_irq |= read_xhfc(hw, R_FILL_BL0 + pt) << (pt * 8);

			/* receive D-Channel Data */
			if (hw->fifo_irq & (1 << (pt*8+5)) ) {
				xhfc_read_fifo_dchan(&hw->port[pt]);
				hw->fifo_irq &= ~(1 << (pt*8+5));
			}

			/* receive B-Channel Data */
			for (bc=0; bc<2; bc++)
				xhfc_read_fifo_bchan(&hw->port[pt], bc);

			/* signel reveive buffers to zaptel */
			zt_receive(&hw->port[pt].zspan); 

			/* reset hdlc index pointers if EOF */
			if (hw->port[pt].zchans[2].eofrx) {
				hw->port[pt].zchans[2].bytes2receive = 0;
				hw->port[pt].zchans[2].eofrx = 0;
			}
			if (hw->port[pt].zchans[2].eoftx) {
				hw->port[pt].zchans[2].bytes2transmit = 0;
				hw->port[pt].zchans[2].eoftx = 0;
			}



			/* e.g. reset ZT_ALARM_RED, if T3 and T4 off */
			if ((hw->port[pt].t3 == HFC_TIMER_OFF) &&
			    (hw->port[pt].t4 == HFC_TIMER_OFF) &&
			    hw->port[pt].zspan.alarms == ZT_ALARM_RED) {
				hw->port[pt].zspan.alarms = ZT_ALARM_NONE;
				zt_alarm_notify(&hw->port[pt].zspan);
			}

			/* handle t3 */
			if (hw->port[pt].t3 > HFC_TIMER_OFF) {
				/* timer expired ? */
				if (--hw->port[pt].t3 == 0) {
					if (debug & DEBUG_HFC_S0_STATES)
						printk (KERN_INFO "%s T3 expired port(%i) state(F%i)\n",
							 hw->port[pt].zspan.name, hw->port[pt].idx,
							 hw->port[pt].su_state.bit.v_su_sta);	

					hw->port[pt].t3 = HFC_TIMER_OFF;
					clear_bit(HFC_L1_ACTIVATING, &hw->port[pt].l1_flags),
					xhfc_ph_command(&hw->port[pt], HFC_L1_FORCE_DEACTIVATE_TE);

					hw->port[pt].zspan.alarms = ZT_ALARM_RED;
					zt_alarm_notify(&hw->port[pt].zspan);
				}
			}

			/* handle t4 */
			if (hw->port[pt].t4 > HFC_TIMER_OFF) {
				/* timer expired ? */
				if (--hw->port[pt].t4 == 0) {
					if (debug & DEBUG_HFC_S0_STATES)
						printk (KERN_INFO "%s T4 expired port(%i) state(F%i)\n",
							 hw->port[pt].zspan.name, hw->port[pt].idx,
							 hw->port[pt].su_state.bit.v_su_sta);	

					hw->port[pt].t4 = HFC_TIMER_OFF;
					
					hw->port[pt].zspan.alarms = ZT_ALARM_RED;
					zt_alarm_notify(&hw->port[pt].zspan);
				}
			}

		}
	}
}

/*********************/
/* Interrupt handler */
/*********************/
static irqreturn_t
xhfc_interrupt(int intno, void *dev_id, struct pt_regs *regs)
{
	xhfc_hw *hw = dev_id;
	__u32 f0_cnt;
	__u8 save_reg_addr;
	unsigned long  flags;


	spin_lock_irqsave(&hw->lock, flags);
	save_reg_addr = read_xhfcregptr(hw);
	
	if (!(hw->irq_ctrl.bit.v_glob_irq_en)
	    && (read_xhfc(hw, R_IRQ_OVIEW))) {
		if (debug & DEBUG_HFC_IRQ)
			printk(KERN_INFO
			       "%s %s NOT M_GLOB_IRQ_EN or R_IRQ_OVIEW \n",
			       hw->card_name, __FUNCTION__);
		return IRQ_NONE;
	}
	
	hw->misc_irq.reg |= read_xhfc(hw, R_MISC_IRQ);

	/* check for desired XHFC interrupt sources */
	if (!(hw->misc_irq.reg & M_TI_IRQ))
		return IRQ_NONE;

	/* count irqs */
	hw->irq_cnt++;

	// directly perform fifo work in interrupt context
	if (!(hw->testirq)) {
		xhfc_irq_work(hw);

#ifdef USE_F0_COUNTER	
		/* akkumulate f0 counter diffs */
		f0_cnt = read_xhfc(hw, R_F0_CNTL);
		f0_cnt += read_xhfc(hw, R_F0_CNTH) << 8;
		hw->f0_akku += (f0_cnt - hw->f0_cnt);
		if ((f0_cnt - hw->f0_cnt) < 0)
			hw->f0_akku += 0xFFFF;
			
		if (hw->f0_cnt == f0_cnt) {
			hw->irq_ctrl.bit.v_glob_irq_en = 0;
			write_xhfc(hw, R_IRQ_CTRL, hw->irq_ctrl.reg);
			WritePCI2PI_u32(hw, PCI2PI_STATUS_INT_ENABLE, 0);
		}
		
		hw->f0_cnt = f0_cnt;
#endif
	}

	
	write_xhfcregptr(hw, save_reg_addr);
	spin_unlock_irqrestore(&hw->lock, flags);
	
	return IRQ_HANDLED;
}

/*****************************************************/
/* disable all interrupts by disabling M_GLOB_IRQ_EN */
/*****************************************************/
static void
disable_interrupts(xhfc_hw * hw)
{
	unsigned long flags;
	
	if (debug & DEBUG_HFC_IRQ)
		printk(KERN_INFO "%s %s\n", hw->card_name, __FUNCTION__);
		
	spin_lock_irqsave(&hw->lock, flags);
	hw->irq_ctrl.bit.v_glob_irq_en = 0;
	write_xhfc(hw, R_IRQ_CTRL, hw->irq_ctrl.reg);
	spin_unlock_irqrestore(&hw->lock, flags);
}

/******************************************/
/* start interrupt and set interrupt mask */
/******************************************/
static void
enable_interrupts(xhfc_hw * hw)
{
	if (debug & DEBUG_HFC_IRQ)
		printk(KERN_INFO "%s %s\n", hw->card_name, __FUNCTION__);

	write_xhfc(hw, R_SU_IRQMSK, 0);

	/* use defined timer interval */
	write_xhfc(hw, R_TI_WD, hw->ti_wd.reg);
	hw->misc_irqmsk.bit.v_ti_irqmsk = 1;
	write_xhfc(hw, R_MISC_IRQMSK, hw->misc_irqmsk.reg);

	/* clear all pending interrupts bits */
	read_xhfc(hw, R_MISC_IRQ);
	read_xhfc(hw, R_SU_IRQ);
	read_xhfc(hw, R_FIFO_BL0_IRQ);
	read_xhfc(hw, R_FIFO_BL1_IRQ);
	read_xhfc(hw, R_FIFO_BL2_IRQ);
	read_xhfc(hw, R_FIFO_BL3_IRQ);

	/* enable global interrupts */
	hw->irq_ctrl.bit.v_glob_irq_en = 1;
	hw->irq_ctrl.bit.v_fifo_irq_en = 1;
	write_xhfc(hw, R_IRQ_CTRL, hw->irq_ctrl.reg);
}


/***********************************/
/* initialise the XHFC ISDN Chip   */
/* return 0 on success.            */
/***********************************/
static int
init_xhfc(xhfc_hw * hw)
{
	int err = 0;
	int timeout = 0x2000;

	hw->chip_id = read_xhfc(hw, R_CHIP_ID);

	if (debug & DEBUG_HFC_INIT)
		printk(KERN_INFO "%s %s ChipID: 0x%x\n", hw->card_name,
		       __FUNCTION__, hw->chip_id);
	
	switch (hw->chip_id) {
		case CHIP_ID_1SU:
			hw->num_ports = 1;
			hw->max_fifo = 4;
			hw->max_z = 0xFF;
			write_xhfc(hw, R_FIFO_MD, M1_FIFO_MD * 2);
			break;
		case CHIP_ID_2SU:
			hw->num_ports = 2;
			hw->max_fifo = 8;
			hw->max_z = 0x7F;
			write_xhfc(hw, R_FIFO_MD, M1_FIFO_MD * 1);
			break;
		case CHIP_ID_2S4U:
		case CHIP_ID_4SU:
			hw->num_ports = 4;
			hw->max_fifo = 16;
			hw->max_z = 0x3F;
			write_xhfc(hw, R_FIFO_MD, M1_FIFO_MD * 0);
			break;
		default:
			err = -ENODEV;
	}

	/* timer irq interval 1 ms */
	hw->ti_wd.bit.v_ev_ts = 0x2;

	if (err) {
		if (debug & DEBUG_HFC_INIT)
			printk(KERN_ERR "%s %s: unkown Chip ID 0x%x\n",
			       hw->card_name, __FUNCTION__, hw->chip_id);
		return (err);
	}

	/* software reset to enable R_FIFO_MD setting */
	write_xhfc(hw, R_CIRM, M_SRES);
	udelay(5);
	write_xhfc(hw, R_CIRM, 0);

	/* amplitude */
	write_xhfc(hw, R_PWM_MD, 0x80);
	write_xhfc(hw, R_PWM1, 0x18);

	write_xhfc(hw, R_FIFO_THRES, 0x11);

	while ((read_xhfc(hw, R_STATUS) & (M_BUSY | M_PCM_INIT))
	       && (timeout))
		timeout--;

	if (!(timeout)) {
		if (debug & DEBUG_HFC_INIT)
			printk(KERN_ERR
			       "%s %s: initialization sequence could not finish\n",
			       hw->card_name, __FUNCTION__);
		return (-ENODEV);
	}

	/* set PCM master mode */
	hw->pcm_md0.bit.v_pcm_md = 1;
	write_xhfc(hw, R_PCM_MD0, hw->pcm_md0.reg);

	/* set pll adjust */
	hw->pcm_md1.bit.v_pll_adj = 3;
	write_xhfc(hw, R_PCM_MD0, hw->pcm_md0.reg + 0x90);
	write_xhfc(hw, R_PCM_MD1, hw->pcm_md1.reg);

	enable_interrupts(hw);

	mdelay(1 << hw->ti_wd.bit.v_ev_ts);
	if (hw->irq_cnt > 2) {
		disable_interrupts(hw);
		hw->testirq = 0;
		return (0);
	} else {
		if (debug & DEBUG_HFC_INIT)
			printk(KERN_INFO
			       "%s %s: ERROR getting IRQ (irq_cnt %i)\n",
			       hw->card_name, __FUNCTION__, hw->irq_cnt);
		disable_interrupts(hw);
		free_irq(hw->irq, hw);
		return (-EIO);
	}
}


/*********************************************/
/* init port (line interface) with SU_CRTLx  */
/*********************************************/
static void
init_su(xhfc_hw * hw, __u8 pt)
{
	xhfc_port_t *port = &hw->port[pt];

	write_xhfc(hw, R_SU_SEL, pt);

	if (port->mode & PORT_MODE_NT)
		port->su_ctrl0.bit.v_su_md = 1;

	if (port->mode & PORT_MODE_EXCH_POL) 
		port->su_ctrl2.reg = M_SU_EXCHG;

	if (port->mode & PORT_MODE_UP) {
		port->st_ctrl3.bit.v_st_sel = 1;
		write_xhfc(hw, A_MS_TX, 0x0F);
		port->su_ctrl0.bit.v_st_sq_en = 1;
	}

	if (debug & DEBUG_HFC_MODE)
		printk(KERN_INFO "%s %s port(%i) "
		       "su_ctrl0(0x%02x) "
		       "su_ctrl1(0x%02x) "
		       "su_ctrl2(0x%02x) "
		       "st_ctrl3(0x%02x)\n",
		       hw->card_name, __FUNCTION__, pt,
		       port->su_ctrl0.reg,
		       port->su_ctrl1.reg,
		       port->su_ctrl2.reg,
		       port->st_ctrl3.reg);

	write_xhfc(hw, A_ST_CTRL3, port->st_ctrl3.reg);
	write_xhfc(hw, A_SU_CTRL0, port->su_ctrl0.reg);
	write_xhfc(hw, A_SU_CTRL1, port->su_ctrl1.reg);
	write_xhfc(hw, A_SU_CTRL2, port->su_ctrl2.reg);
	
	if (port->mode & PORT_MODE_TE)
		write_xhfc(hw, A_SU_CLK_DLY, CLK_DLY_TE);
	else
		write_xhfc(hw, A_SU_CLK_DLY, CLK_DLY_NT);
}

/*********************************************************/
/* Setup Fifo using A_CON_HDLC, A_SUBCH_CFG, A_FIFO_CTRL */
/*********************************************************/
static void
setup_fifo(xhfc_hw * hw, __u8 fifo, __u8 conhdlc, __u8 subcfg,
	   __u8 fifoctrl, __u8 enable)
{
	xhfc_selfifo(hw, fifo);
	write_xhfc(hw, A_CON_HDLC, conhdlc);
	write_xhfc(hw, A_SUBCH_CFG, subcfg);
	write_xhfc(hw, A_FIFO_CTRL, fifoctrl);

	if (enable)
		hw->fifo_irqmsk |= (1 << fifo);
	else
		hw->fifo_irqmsk &= ~(1 << fifo);

	xhfc_resetfifo(hw);
	xhfc_selfifo(hw, fifo);

	if (debug & DEBUG_HFC_MODE) {
		printk(KERN_INFO
		       "%s %s: fifo(%i) conhdlc(0x%02x) subcfg(0x%02x) fifoctrl(0x%02x)\n",
		       hw->card_name, __FUNCTION__, fifo,
		       sread_xhfc(hw, A_CON_HDLC),
		       sread_xhfc(hw, A_SUBCH_CFG),
		       sread_xhfc(hw, A_FIFO_CTRL)
		    );
	}
}

/**************************************************/
/* Setup S/U interface, enable/disable B-Channels */
/**************************************************/
static void
setup_su(xhfc_hw * hw, __u8 pt, __u8 bc, __u8 enable)
{
	xhfc_port_t *port = &hw->port[pt];

	if (!((bc == 0) || (bc == 1))) {
		printk(KERN_INFO "%s %s: pt(%i) ERROR: bc(%i) unvalid!\n",
		       hw->card_name, __FUNCTION__, pt, bc);
		return;
	}

	if (debug & DEBUG_HFC_MODE)
		printk(KERN_INFO "%s %s %s pt(%i) bc(%i)\n",
		       hw->card_name, __FUNCTION__,
		       (enable) ? ("enable") : ("disable"), pt, bc);

	if (bc) {
		port->su_ctrl2.bit.v_b2_rx_en = (enable?1:0);
		port->su_ctrl0.bit.v_b2_tx_en = (enable?1:0);
	} else {
		port->su_ctrl2.bit.v_b1_rx_en = (enable?1:0);
		port->su_ctrl0.bit.v_b1_tx_en = (enable?1:0);
	}

	if (hw->port[pt].mode & PORT_MODE_NT)
		hw->port[pt].su_ctrl0.bit.v_su_md = 1;

	write_xhfc(hw, R_SU_SEL, pt);
	write_xhfc(hw, A_SU_CTRL0, hw->port[pt].su_ctrl0.reg);
	write_xhfc(hw, A_SU_CTRL2, hw->port[pt].su_ctrl2.reg);
}

/********************************/
/* parse module paramaters like */
/* NE/TE and S0/Up port mode    */
/********************************/
static void
parse_module_params(xhfc_hw * hw)
{
	__u8 pt;

	/* parse module parameters */
	for (pt = 0; pt < hw->num_ports; pt++) {
		/* Line Interface TE or NT */
		if (protocol[hw->param_idx + pt] & 0x01)
			hw->port[pt].mode |= PORT_MODE_NT;
		else
			hw->port[pt].mode |= PORT_MODE_TE;

		/* Line Interface in S0 or Up mode */
		if (protocol[hw->param_idx + pt] & 0x02)
			hw->port[pt].mode |= PORT_MODE_UP;
		else
			hw->port[pt].mode |= PORT_MODE_S0;

		/* st line polarity */
		if (protocol[hw->param_idx + pt] & 0x04)
			hw->port[pt].mode |= PORT_MODE_EXCH_POL;
			

		if (debug & DEBUG_HFC_INIT)
			printk ("%s %s: protocol[%i]=0x%02x, mode:%s,%s %s\n",
			        hw->card_name, __FUNCTION__, hw->param_idx+pt,
			        protocol[hw->param_idx + pt],
			        (hw->port[pt].mode & PORT_MODE_TE)?"TE":"NT",
			        (hw->port[pt].mode & PORT_MODE_S0)?"S0":"Up",
			        (hw->port[pt].mode & PORT_MODE_EXCH_POL)?"SU_EXCH":""
			        );
	}
}

/********************************/
/* initialise the XHFC hardware */
/* return 0 on success.         */
/********************************/
static int __devinit
setup_instance(xhfc_hw * hw)
{
	int err;
	int pt;

#if BRIDGE == BRIDGE_PCI2PI
	err = init_pci_bridge(hw);
	if (err)
		goto out;
#endif

	if (debug & DEBUG_HFC_INIT)
		printk(KERN_WARNING "%s %s: requesting IRQ %d\n",
		       hw->card_name, __FUNCTION__, hw->irq);

	spin_lock_init(&hw->lock);

	/* init interrupt engine */
	hw->testirq = 1;
	if (request_irq(hw->irq, xhfc_interrupt, SA_SHIRQ, DRIVER_NAME, hw)) {
		printk(KERN_WARNING "%s %s: couldn't get interrupt %d\n",
		       hw->card_name, __FUNCTION__, hw->irq);
		hw->irq = 0;
		err = -EIO;
		goto out;
	}

	err = init_xhfc(hw);
	if (err)
		goto out;

	parse_module_params(hw);

	/* init line interfaces */
	for (pt = 0; pt < hw->num_ports; pt++) {
		/* init back pointers */
		hw->port[pt].idx = pt;
		hw->port[pt].hw = hw;
		
		/* init port as TE/NT */
		init_su(hw, pt);

		hw->port[pt].t3 = -1;	/* init t3 timer : Timer OFF */
		hw->port[pt].t4 = -1;	/* init t4 timer : Timer OFF */

		err = init_zaptel_interface(&hw->port[pt]);
		if (err)
			goto out;
	}
	
	enable_interrupts(hw);

	return (0);

      out:
	return (err);
}

/************************/
/* release single card  */
/************************/
static void
release_card(xhfc_hw * hw)
{
	__u8 pt;

	disable_interrupts(hw);
	free_irq(hw->irq, hw);

	for (pt = 0; pt < hw->num_ports; pt++)
		if (hw->port[pt].zspan.flags & ZT_FLAG_REGISTERED) {
			if (debug & DEBUG_HFC_INIT) 
				printk (KERN_INFO "%s zt_unregister %s\n",
				        __FUNCTION__, hw->port[pt].zspan.name);
			zt_unregister(&hw->port[pt].zspan);
		}

	mdelay(100);
	kfree(hw);
}

#if BRIDGE == BRIDGE_PCI2PI

/*****************************************/
/* PCI hotplug interface: probe new card */
/*****************************************/
static int __devinit
xhfc_pci_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	xhfc_param *driver_data = (xhfc_param *) ent->driver_data;
	xhfc_hw *hw;
	int err = -ENOMEM;

	if (!(hw = kmalloc(sizeof(xhfc_hw), GFP_ATOMIC))) {
		printk(KERN_ERR "%s %s: No kmem for XHFC card\n",
		       hw->card_name, __FUNCTION__);
		return (err);
	}
	memset(hw, 0, sizeof(xhfc_hw));

	hw->pdev = pdev;
	err = pci_enable_device(pdev);

	if (err)
		goto out;

	hw->cardnum = card_cnt;
	sprintf(hw->card_name, "%s_%d", DRIVER_NAME, hw->cardnum);
	printk(KERN_INFO "%s %s: adapter '%s' found on PCI bus %02x dev %02x\n",
	       hw->card_name, __FUNCTION__, driver_data->device_name,
	       pdev->bus->number, pdev->devfn);

	hw->driver_data = *driver_data;
	hw->irq = pdev->irq;

	hw->hw_membase = (u_char *) pci_resource_start(pdev, 1);
	hw->membase = ioremap((ulong) hw->hw_membase, 4096);

	pci_set_drvdata(pdev, hw);
	err = setup_instance(hw);
	if (!err) {
		card_cnt++;
		return (0);
	} else {
		goto out;
	}

      out:
	kfree(hw);
	return (err);
};

/**************************************/
/* PCI hotplug interface: remove card */
/**************************************/
static void __devexit
xhfc_pci_remove(struct pci_dev *pdev)
{
	xhfc_hw *hw = pci_get_drvdata(pdev);
	printk(KERN_INFO "%s %s: removing card\n", hw->card_name,
	       __FUNCTION__);
	release_card(hw);
	card_cnt--;
	pci_disable_device(pdev);
	return;
};


static struct pci_device_id xhfc_ids[] = {
	{.vendor = PCI_VENDOR_ID_CCD,
	 .device = 0xA003,
	 .subvendor = 0x1397,
	 .subdevice = 0xA003,
	 .driver_data =
	 (unsigned long) &((xhfc_param) {CHIP_ID_4SU, "XHFC Evaluation Board"}),
	 },
	{}
};

/***************/
/* Module init */
/***************/
static struct pci_driver xhfc_driver = {
      name:DRIVER_NAME,
      probe:xhfc_pci_probe,
      remove:__devexit_p(xhfc_pci_remove),
      id_table:xhfc_ids,
};


MODULE_DEVICE_TABLE(pci, xhfc_ids);

#endif // BRIDGE_PCI2PI

/***************/
/* Module init */
/***************/
static int __init
xhfc_init(void)
{
	int err;

	printk(KERN_INFO "XHFC: %s driver %s (debug=%i)\n",
	       __FUNCTION__, xhfc_rev, debug);

#if BRIDGE == BRIDGE_PCI2PI
	err = pci_register_driver(&xhfc_driver);
	if (err < 0) {
		goto out;
	}
#endif

	printk(KERN_INFO "XHFC: %d cards installed\n", card_cnt);

#if !defined(CONFIG_HOTPLUG)
	if (err == 0) {
		err = -ENODEV;
		pci_unregister_driver(&xhfc_driver);
		goto out;
	}
#endif

	return 0;

      out:
	return (err);
}

static void __exit
xhfc_cleanup(void)
{
#if BRIDGE == BRIDGE_PCI2PI
	pci_unregister_driver(&xhfc_driver);
#endif

	printk(KERN_INFO "%s: driver removed\n", __FUNCTION__);
}

module_init(xhfc_init);
module_exit(xhfc_cleanup);
