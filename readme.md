# Input Latency _(with a game this time)_

Previous latency measurements were taken with a simple 'blank' application which flashed the whole screen/window black or white depending on the joypad button. This is not a valid real world test because the streaming systems (Shadow, Parsec, Polystream) were not required to do much work to get the frame compressed and sent. It's not valid for native measurements either because the CPU/GPU load was negligible which isn't representative of a normal game running.

So we need a way to test input latency when running more realistic workloads. To do this, a simple D3D Hook application inserts itself into the `Present()` method and displays a small rectangle of black or white depending on the joypad button state. In this way we can run real world workloads (games, basically) and still measure the end-to-end latency.

#### Method

- Select a game, note whether it uses DX9/DX11 and is 32/64 bit
- Kick off the benchmark and start the measurement
- Stop the measurement when the benchmark ends
- Use the same PC, same Monitor, same everything across all tests
- TODO: Joypad Y pauses and resumes (or just ends?) the measurement

The method in the hooked `Present()` function is as follows:

```
static var button_state

hooked_present()
{
    var color
    if(button_state == pressed) color = white else color = black
    draw_rectangle(color)
    var return_value = original_present()
    button_state = get_button_state()
    return return_value
}
```
Note that the pad state is read _after_ the call to `original_present()` and used to determine the fillrect color _next_ frame. This is so that the sequence of events closely matches the normal game input reading sequence.

#### Setup

Initially, two machine setups are used

    1   Desktop:    Intel Core i7 @ 3.5GHz, nVidia GTX 1060Ti, Monitor TBD
    2   Laptop:     Intel Core i5 @ 2.2GHz, Intel 640, built-in laptop monitor

#### Instructions

- Run the correct (x86 or x64) version
- Select D3D9 or D3D11 mode as appropriate
- Run the game
- Wait until the game is doing what you want to measure
- Press `BACKTICK` (aka the tilde key, the one above the TAB key)
- If you don't see a black rectangle in the top left corner:
    - is there a joypad plugged in and registering correctly? (rectangle should be red in this case)
    - is `XInput1_3.dll` available (rectangle should be cyan in this case)
    - have you used the right bitness / dx version?
    - check the hooker log window for any errors
- If you do see a black rectangle, it should go white when you press the joypad button
- If it doesn't go white when you press the joypad button, check the joypad is working in the control panel applet
- If it's working, take measurements with your timing gadget of choice


##### XInput1_3.dll:

`XInput1_3.dll` (64 bit version) must exist in `%WINDIR%\System32`<br>
`XInput1_3.dll` (32 bit version) must exist in `%WINDIR%\SysWOW64`

##### Shadow and Parsec

If the remote controller isn't recognised, quit everything, unplug it, plug it back in, get the 'USB Game Controller' control panel applet up and check it's present

