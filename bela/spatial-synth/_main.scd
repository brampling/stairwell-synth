/*
Stage 2 SuperCollider patch for the Bela Gem Multi.

Runs as a Bela SC project: place this file as _main.scd inside a project folder
under ~/Bela/projects/ and run with `make -C ~/Bela run PROJECT=<name>`. When a
project contains a _main.scd, Bela starts sclang and executes it.

It reads the receiver's ASCII line stream from the UART (/dev/ttyS4) and:
  - fires a percussive "ping" on switch down (S1)
  - moves the drone frequency from the pot value (P<n>)

The line format must match firmware/include/protocol.h via the receiver:
  S1 / S0  switch down / up        P<n>  pot value 0..4095
*/

s = Server.default;

// Bela audio configuration. Stereo out is enough for Stage 2; the Gem Multi's
// extra outputs come in at Stage 4 with the speaker array.
s.options.numAnalogInChannels = 8;
s.options.numAnalogOutChannels = 8;
s.options.numDigitalChannels = 16;
s.options.blockSize = 16;
s.options.numInputBusChannels = 2;
s.options.numOutputBusChannels = 2;

s.waitForBoot({
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

	// Note the SerialPort API: stopbit: false means ONE stop bit (true would set
	// two, which frame-errors against the receiver's 8N1 output). 8N1, no flow
	// control, to match the firmware.
	port = SerialPort("/dev/ttyS4",
		baudrate: 115200,
		databits: 8,
		stopbit: false,
		parity: nil,
		crtscts: false);

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

	"listening on /dev/ttyS4".postln;
});

ServerQuit.add({ 0.exit }); // quit sclang when the Bela button is pressed
