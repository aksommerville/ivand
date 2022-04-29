/* InputManager.js
 */
 
export class InputManager {
  constructor() {
    this.buttons = 0;
    this.gamepads = [];
    this._initKeyboard();
    this._initGamepad();
  }
  
  update() {
    this._updateGamepads();
    return this.buttons;
  }
  
  /* Private.
   **********************************************************************/
   
  _setButton(bit, value) {
    if (value) this.buttons |= bit;
    else this.buttons &= ~bit;
  }
   
  _initKeyboard() {
    window.addEventListener("keydown", (event) => this._onKey(event, true));
    window.addEventListener("keyup", (event) => this._onKey(event, false));
  }
  
  _onKey(event, value) {
    if (event.repeat) return;
    switch (event.key) {
      case "ArrowLeft": this._setButton(0x01, value); return;
      case "ArrowRight": this._setButton(0x02, value); return;
      case "ArrowUp": this._setButton(0x04, value); return;
      case "ArrowDown": this._setButton(0x08, value); return;
      case "z": case "a": this._setButton(0x10, value); return;
      case "x": case "b": this._setButton(0x20, value); return;
    }
  }
  
  _initGamepad() {
    window.addEventListener("gamepadconnected", (event) => this._onConnect(event));
    window.addEventListener("gamepaddisconnected", (event) => this._onDisconnect(event));
  }
  
  _onConnect(event) {
    // This is for my Xbox joystick. TODO general input mapping
    event.gamepad._ivand_map = {
      axes: [ [6, 0x01, 0x02, 0], [7, 0x04, 0x08, 0] ],
      buttons: [ [0, 0x10, 0], [2, 0x20, 0] ],
    };
    this.gamepads.push(event.gamepad);
  }
  
  _onDisconnect(event) {
    const p = this.gamepads.indexOf(event.gamepad);
    if (p >= 0) {
      this.gamepads.splice(p, 1);
      this.buttons = 0;
    }
  }
  
  _updateGamepads() {
    for (const gamepad of this.gamepads) {
      if (gamepad._ivand_map) {
        for (const axis of gamepad._ivand_map.axes) {
          const nv = gamepad.axes[axis[0]];
          if (nv <= -0.5) {
            if (axis[3] >= 0) {
              axis[3] = -1;
              this.buttons &= ~axis[2];
              this.buttons |= axis[1];
            }
          } else if (nv >= 0.5) {
            if (axis[3] <= 0) {
              axis[3] = 1;
              this.buttons &= ~axis[1];
              this.buttons |= axis[2];
            }
          } else if (axis[3]) {
            axis[3] = 0;
            this.buttons &= ~(axis[1] | axis[2]);
          }
        }
        for (const button of gamepad._ivand_map.buttons) {
          const buttonobj = gamepad.buttons[button[0]];
          if (!buttonobj) continue;
          const nv = buttonobj.value;
          if (nv !== button[2]) {
            button[2] = nv;
            if (nv) this.buttons |= button[1];
            else this.buttons &= ~button[1];
          }
        }
      }
    }
  }
}
