// Stage 2 SuperCollider patch for the Bela Gem Multi.
//
// Reads the receiver's ASCII line stream from the UART and:
//   - fires a percussive "ping" on switch down (S1)
//   - moves the drone frequency from the pot value (P<n>)
//
// This runs in sclang (SerialPort is a language-side class). On the Gem, open
// this in the browser IDE or run it over Remote-SSH. If you wire the receiver
// straight into the Bela USB-A port instead of the UART pins, change the device
// to "/dev/ttyUSB0".
//
// The wire format here must match firmware/include/protocol.h via the receiver:
//   S1 / S0  switch down / up        P<n>  pot value 0..4095

(
s.waitForBoot {
	var port, drone, buf = "", handleLine;

	SynthDef(\drone, { |freq = 220, amp = 0.15|
		var sig = SinOsc.ar(freq) * amp;
		Out.ar(0, sig ! 2);
	}).add;

	SynthDef(\ping, { |freq = 880, amp = 0.3|
		var env = EnvGen.kr(Env.perc(0.005, 0.25), doneAction: 2);
		Out.ar(0, SinOsc.ar(freq) * env * amp ! 2);
	}).add;

	s.sync;
	drone = Synth(\drone);

	handleLine = { |line|
		case
		{ line == "S1" } {
			"switch down -> ping".postln;
			Synth(\ping);
		}
		{ line == "S0" } {
			"switch up".postln;
		}
		{ line[0] == $P } {
			var raw  = line.copyRange(1, line.size - 1).asInteger;
			var freq = raw.linexp(0, 4095, 110, 880);
			("pot " ++ raw ++ " -> " ++ freq.round(0.1) ++ " Hz").postln;
			drone.set(\freq, freq);
		};
	};

	port = SerialPort("/dev/ttyS4", baudrate: 115200, crtscts: false);

	// Read byte by byte, dispatch on each complete newline-terminated line.
	Routine({
		var ch;
		loop {
			ch = port.read.asAscii;
			if ((ch == $\n) or: { ch == $\r }) {
				if (buf.size > 0) { handleLine.(buf); buf = ""; };
			} {
				buf = buf ++ ch;
			};
		};
	}).play;

	// Tidy up the port on Cmd-period so re-running does not leave it locked.
	CmdPeriod.doOnce({ port.close; "serial port closed".postln; });

	"listening on serial".postln;
};
)
