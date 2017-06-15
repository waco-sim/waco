package d3s.cooja.plugins;

import java.io.IOException;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.util.Observable;
import java.util.Observer;
import java.util.Properties;
import java.util.Collection;
import java.util.ArrayList;
import org.jdom.Element;

import org.contikios.cooja.ClassDescription;
import org.contikios.cooja.Cooja;
import org.contikios.cooja.Plugin;
import org.contikios.cooja.PluginType;
import org.contikios.cooja.RadioConnection;
import org.contikios.cooja.RadioMedium;
import org.contikios.cooja.RadioPacket;
import org.contikios.cooja.Simulation;
import org.contikios.cooja.VisPlugin;
import org.contikios.cooja.interfaces.Radio;
//import org.contikios.cooja.plugins.analyzers.PacketAnalyser;
//import org.contikios.cooja.plugins.analyzers.PcapExporter;

import org.contikios.cooja.util.StringUtils;

/**
 * Radio Logger for Cooja's headless mode
 *
 * Based on Laurent Deru's PCap logger
 *
 */
@ClassDescription("Headless radio logger")
@PluginType(PluginType.SIM_PLUGIN)
public class RadioLoggerHeadless extends VisPlugin {
  private static final long serialVersionUID = -6927191711697081353L;

  private final Simulation simulation;
  private RadioMedium radioMedium;
  private Observer radioMediumObserver;
  private BufferedWriter logWriter;

  public RadioLoggerHeadless(final Simulation simulationToControl, final Cooja gui) {
    super("Radio messages", gui, false);
    System.err.println("Starting headless radio logger");
    
	try {
		logWriter = new BufferedWriter(new FileWriter(new File("radio.log")));

    } catch (IOException e) {
        e.printStackTrace();
    }
    simulation = simulationToControl;
    radioMedium = simulation.getRadioMedium();

    radioMedium.addRadioTransmissionObserver(radioMediumObserver = new Observer() {
      public void update(Observable obs, Object obj) {
        RadioConnection conn = radioMedium.getLastConnection();
    	byte[] data;
        
		if (conn == null) {
          return;
		}
        RadioPacket radioPacket = conn.getSource().getLastPacketTransmitted();
		if (radioPacket == null)
			return;
        long endTime = simulation.getSimulationTime();

		data = radioPacket.getPacketData();
		StringBuffer out = new StringBuffer(data.length*2);

		for (byte b : data) {
			out.append(StringUtils.toHex(b));
		}

        try {
			Radio[] dests = conn.getDestinations(); // all non-interferred destinations
			StringBuffer s = new StringBuffer();

			for (Radio r:dests) {
				s.append(r.getMote().getID());
				s.append(",");
			}
			int l = s.length();
			if (l>0)
				s.delete(l-1,l);
			logWriter.write(Long.toString(endTime) + "\t" + conn.getSource().getMote().getID() + "\t" + out + "\t[" + s + "]\n");
			// write
        } catch (IOException e) {
            System.err.println("Could not export packet data");
            e.printStackTrace();
        }
      }
    });
  }

  public void closePlugin() {
    System.err.println("Stopping headless radio logger");
	try {
	  logWriter.close();
	} catch (IOException e) {
		System.err.println("Could not close the file");
		e.printStackTrace();
	}
    if (radioMediumObserver != null) {
      radioMedium.deleteRadioTransmissionObserver(radioMediumObserver);
    }
  }
  public boolean setConfigXML(Collection<Element> configXML, boolean visAvailable) {
	    /*System.err.println("RadioLogger.setConfigXML()");
	    for (Element element : configXML) {
	        String name = element.getName();
	        if (name.equals("pcap_file")) {
	          pcapFile = simulation.getGUI().restorePortablePath(new File(element.getText()));
	          try {
		          pcapExporter.openPcap(pcapFile);
	          } catch (IOException e) {
	              e.printStackTrace();
	          }
	        }
	      }*/
	    return true;
	  }

	  public Collection<Element> getConfigXML() {
		    System.err.println("RadioLogger.getConfigXML()");
		    ArrayList<Element> config = new ArrayList<Element>();
		    Element element;

		    /*if (pcapFile != null) {
		      element = new Element("pcap_file");
		      File file = simulation.getGUI().createPortablePath(pcapFile);
		      element.setText(pcapFile.getPath().replaceAll("\\\\", "/"));
		      element.setAttribute("EXPORT", "copy");
		      config.add(element);
		    }*/

		    return config;
	  }

}
