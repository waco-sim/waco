
package org.contikios.cooja.mspmote.interfaces;

import java.util.Collection;

import org.apache.log4j.Logger;
import org.jdom.Element;

import org.contikios.cooja.ClassDescription;
import org.contikios.cooja.Mote;
import org.contikios.cooja.RadioPacket;
import org.contikios.cooja.Simulation;
import org.contikios.cooja.interfaces.CustomDataRadio;
import org.contikios.cooja.interfaces.Position;
import org.contikios.cooja.interfaces.Radio;
import org.contikios.cooja.mspmote.SkyMote;
import org.contikios.cooja.mspmote.MspMoteTimeEvent;
import se.sics.mspsim.chip.CC2420;
import se.sics.mspsim.chip.ChannelListener;
import se.sics.mspsim.chip.RFListener;
import se.sics.mspsim.chip.Radio802154;
import se.sics.mspsim.core.Chip;
import se.sics.mspsim.chip.WurChip;
import se.sics.mspsim.core.OperatingModeListener;
import se.sics.mspsim.platform.sky.SkyNode;
import se.sics.mspsim.core.PortListener;
import se.sics.mspsim.core.IOPort;
import org.contikios.cooja.mote.memory.MemoryInterface;
import org.contikios.cooja.TimeEvent;

/**
 * MSPSim WakeupRadio to COOJA wrapper.
 *
 */
@ClassDescription("Custom Wakeup radio")
public class WakeupRadio extends Radio implements CustomDataRadio {

  private static Logger logger = Logger.getLogger(WakeupRadio.class);
  /**
   * Inter-byte delay for delivering wake-up packet bytes.
   */
  public static final long DELAY_BETWEEN_BYTES =
    (long) (1000.0*Simulation.MILLISECOND/(100000.0/8.0)); /* us. Corresponds to 100kbit/s */

  private RadioEvent lastEvent = RadioEvent.UNKNOWN;
  private boolean isInterfered = false;
  private boolean isTransmitting = false;
  private boolean isReceiving = false;
  final SkyMote mote;

  private Object lastOutgoingData = "Hello";
  private WurChip wurChip;

  public WakeupRadio(Mote m) {
    this.mote = (SkyMote)m;
		
	wurChip = mote.getCPU().getChip(WurChip.class);
    if (wurChip == null) {
      throw new IllegalStateException("Mote is not equipped with a wakeup radio");
    }
	wurChip.port.addPortListener(new PortListener() {
		 public void portWrite(IOPort source, int data) {
		   if ((data & (1 << wurChip.pin_tx)) != 0) {
		  // we transmit here
			//logger.fatal("WUR: Start transmission");
			MemoryInterface mi = WakeupRadio.this.mote.getMemory();
			MemoryInterface.Symbol dst_address_len = mi.getSymbolMap().get("WUR_TX_LENGTH");
			MemoryInterface.Symbol dst_address = mi.getSymbolMap().get("WUR_TX_BUFFER");

			if (dst_address_len != null) {
				lastEvent = RadioEvent.TRANSMISSION_STARTED;
				isTransmitting = true;
				setChanged();
				notifyObservers();
			
				final int len = mi.getMemorySegment(dst_address_len.addr, 1)[0];
				final byte v[] = mi.getMemorySegment(dst_address.addr, len);

				/*StringBuffer addr_str = new StringBuffer();
				for (int i=0; i<len; i++)
					addr_str.append(v[i]).append(":");
				logger.fatal("WUR: Sending packet of length " + len + ": " + addr_str);
				*/
				mote.getSimulation().scheduleEvent(new TimeEvent(0) {
					public void execute(long t) {
						lastOutgoingData = v;
						lastEvent = RadioEvent.CUSTOM_DATA_TRANSMITTED;
						setChanged();
						notifyObservers();

						//logger.fatal("WUR: Finish transmission");
						isTransmitting = false;
						lastEvent = RadioEvent.TRANSMISSION_FINISHED;
						setChanged();
						notifyObservers();
						mote.requestImmediateWakeup();
					}
				},
				mote.getSimulation().getSimulationTime() + DELAY_BETWEEN_BYTES*len); // TODO the WUR packet duration should be configurable
			} 
			else {
				logger.fatal("WUR: no destination! doing nothing. ");
			}
		   }
	  }
	});
  }

  public void attachToNode() {
  }

  
  /* Custom data radio support */
  public Object getLastCustomDataTransmitted() {
    return lastOutgoingData;
  }

  public Object getLastCustomDataReceived() {
    return "BOO";
  }

  public void receiveCustomData(Object data) {
    if (isInterfered) {
  	  logger.fatal("WUR: corrupted due to interference");
	  return;
    }
	
	byte[] buf = (byte[])data;
	/*StringBuffer addr_str = new StringBuffer();
	for (int i=0; i<addr.length; i++)
		addr_str.append(addr[i]).append(":");
    logger.fatal("WUR: Received custom data, length: " + addr.length + ": " + addr_str);
	*/
	
    // put the address to the WUR buffer of the node
	if (buf.length > 0) { // just a workaround (during the initialisation phase the WUR is mistakenly activated but the len=0)
		MemoryInterface mi = WakeupRadio.this.mote.getMemory();
		MemoryInterface.Symbol dst_address_len = mi.getSymbolMap().get("WUR_RX_LENGTH");
		MemoryInterface.Symbol dst_address = mi.getSymbolMap().get("WUR_RX_BUFFER");
		mi.setMemorySegment(dst_address_len.addr, new byte[] {(byte)buf.length});
		mi.setMemorySegment(dst_address.addr, buf);
    	wurChip.wakeup();
	}
  }
  
  public void interfereAnyReception() {
   	//logger.fatal("WUR: interfereAnyReception");
    isInterfered = true;
    isReceiving = false;
	

    lastEvent = RadioEvent.RECEPTION_INTERFERED;
    //logger.debug("----- 802.15.4 RECEPTION INTERFERED -----");
    setChanged();
    notifyObservers();
  }

  public Mote getMote() {
    return mote;
  }
  
  public boolean canReceiveFrom(CustomDataRadio radio) {
   	//logger.fatal("WUR: canReceiveFrom");
    if (radio.getClass().equals(this.getClass())) {
      return true;
    }
    return false;
  }

  public Position getPosition() {
    return mote.getInterfaces().getPosition();
  }

  public int getChannel() {
    //return radio.getActiveChannel();
    return 1;
  }

  public int getFrequency() {
    //return radio.getActiveFrequency();
    return 0;
  }
  
  public double getCurrentSignalStrength() {
    //return currentSignalStrength;
	return 0;
  }

  public void setCurrentSignalStrength(final double signalStrength) {
  }

  public double getCurrentOutputPower() {
    //return radio.getOutputPower();
	return 1;
  }

  public int getCurrentOutputPowerIndicator() {
    //return radio.getOutputPowerIndicator();
    return 1;
  }

  public int getOutputPowerIndicatorMax() {
    //return radio.getOutputPowerIndicatorMax();
    return 1;
  }

  public boolean isRadioOn() {
      return true;
  }

  public boolean isInterfered() {
    //logger.fatal("WUR: isInterfered");
    return isInterfered;
  }

  public boolean isTransmitting() {
    //logger.fatal("WUR: isTransmitting");
    return isTransmitting;
  }

  public boolean isReceiving() {
    //logger.fatal("WUR: isReceiving");
    return isReceiving;
  }
  
  public RadioEvent getLastEvent() {
    //logger.fatal("WUR: getLastEvent");
    return lastEvent;
  }
  
  public void signalReceptionStart() {
    //logger.fatal("WUR: signalReceptionStart");
    isReceiving = true;

    lastEvent = RadioEvent.RECEPTION_STARTED;
    /*logger.debug("----- 802.15.4 RECEPTION STARTED -----");*/
    setChanged();
    notifyObservers();
  }

  public void signalReceptionEnd() {
    //logger.fatal("WUR: signalReceptionEnd");
    /* Deliver packet data */
    isReceiving = false;
    isInterfered = false;

    lastEvent = RadioEvent.RECEPTION_FINISHED;
    /*logger.debug("----- 802.15.4 RECEPTION FINISHED -----");*/
    setChanged();
    notifyObservers();
  }
  
  public RadioPacket getLastPacketTransmitted() {
    return null;
  }

  public RadioPacket getLastPacketReceived() {
    return null;
  }
  
  public void setReceivedPacket(RadioPacket packet) {
  }

  public void setConfigXML(Collection<Element> configXML, boolean visAvailable) {
  }
  
  public Collection<Element> getConfigXML() {
    return null;
  }
}
