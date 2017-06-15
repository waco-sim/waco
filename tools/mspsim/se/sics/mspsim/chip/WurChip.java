package se.sics.mspsim.chip;
import se.sics.mspsim.core.Chip;
import se.sics.mspsim.core.IOPort;
import se.sics.mspsim.core.MSP430Core;
import se.sics.mspsim.core.PortListener;
import se.sics.mspsim.core.TimeEvent;


public class WurChip extends Chip {
    public final IOPort port;
    public final int pin_rx;
    public final int pin_tx;

    public WurChip(String id, MSP430Core cpu, IOPort port, int pin_rx, int pin_tx) {
        super(id, cpu);
        this.port = port;
		this.pin_rx = pin_rx;
		this.pin_tx = pin_tx;

        port.setPinState(pin_rx, IOPort.PinState.LOW);
    }

	// this is called from the cooja WakeupRadio interface when a wakeup packet arrives
	public void wakeup()
	{
		//System.out.println("pin_rx -> HI");
        port.setPinState(pin_rx, IOPort.PinState.HI);
		// here an interrupt should be fired (is it done automatically?)
		
        port.setPinState(pin_rx, IOPort.PinState.LOW);
      	/*cpu.scheduleCycleEvent(new TimeEvent(10) {
        public void execute(long t) {
		  System.out.println("pin_rx -> LOW");
          port.setPinState(pin_rx, IOPort.PinState.LOW);
        }
      }, cpu.getTime() + 10); */
	}

	
    @Override
    public int getConfiguration(int parameter) {
        return 0;
    }

    @Override
    public int getModeMax() {
        return 0;
    }

    @Override
    public String info() {
        return "No info";
    }
}
