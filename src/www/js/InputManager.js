/* InputManager.js
 */
 
export class InputManager {
  constructor() {
    this.buttons = 0;
    this.gamepads = [];
    this.touch = null; // { identifier,clientX,clientY,usage:"a,b,d,x" }
    this.touchDpad = [0, 0];
    this.touchA = false;
    this.touchB = false;
    this.touchDpadDistance = 100;
    this._initKeyboard();
    this._initGamepad();
    this._initTouch();
  }
  
  update() {
    this._updateGamepads();
    return this.buttons | this._getTouchButtons();
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
  
  _getTouchButtons() {
    let buttons = 0;
    if (this.touchA) buttons |= 0x10;
    if (this.touchB) buttons |= 0x20;
    if (this.touchDpad[0] < 0) buttons |= 0x01;
    else if (this.touchDpad[0]> 0) buttons |= 0x02;
    if (this.touchDpad[1] < 0) buttons |= 0x04;
    else if (this.touchDpad[1]> 0) buttons |= 0x08;
    return buttons;
  }
  
  _initTouch() {
    const dpad = document.getElementById("touch-dpad");
    dpad.addEventListener("touchstart", (event) => this.onDpadTouchStart(event));
    dpad.addEventListener("touchend", (event) => this.onDpadTouchEnd(event));
    dpad.addEventListener("touchcancel", (event) => this.onDpadTouchEnd(event));
    dpad.addEventListener("touchmove", (event) => this.onDpadTouchMove(event));
    const a = document.getElementById("touch-a");
    a.addEventListener("touchstart", (event) => this.onTouchButton(event, "a", true));
    a.addEventListener("touchend", (event) => this.onTouchButton(event, "a", false));
    a.addEventListener("touchcancel", (event) => this.onTouchButton(event, "a", false));
    const b = document.getElementById("touch-b");
    b.addEventListener("touchstart", (event) => this.onTouchButton(event, "b", true));
    b.addEventListener("touchend", (event) => this.onTouchButton(event, "b", false));
    b.addEventListener("touchcancel", (event) => this.onTouchButton(event, "b", false));
  }
  
  onDpadTouchStart(event) {
    event.preventDefault();
    if (this.touch) return;
    for (const touch of event.touches) {
      this.touch = {
        identifier: touch.identifier,
        clientX: touch.clientX,
        clientY: touch.clientY,
      };
    }
  }
  
  onDpadTouchEnd(event) {
    event.preventDefault();
    if (!this.touch) return;
    for (const touch of event.changedTouches) {
      if (touch.identifier === this.touch.identifier) {
        this.touch = null;
        this.touchDpad = [0, 0];
        return;
      }
    }
  }
  
  onDpadTouchMove(event) {
    event.preventDefault();
    if (!this.touch) return;
    for (const touch of event.touches) {
      if (touch.identifier !== this.touch.identifier) continue;
      const dx = touch.clientX - this.touch.clientX;
      const dy = touch.clientY - this.touch.clientY;
      if (dx > this.touchDpadDistance) this.touchDpad[0] = 1;
      else if (dx < -this.touchDpadDistance) this.touchDpad[0] = -1;
      else this.touchDpad[0] = 0;
      if (dy > this.touchDpadDistance) this.touchDpad[1] = 1;
      else if (dy < -this.touchDpadDistance) this.touchDpad[1] = -1;
      else this.touchDpad[1] = 0;
    }
  }
  
  onTouchButton(event, button, value) {
    event.preventDefault();
    console.log(`onTouchButton`, { event, button, value});
    switch (button) {
      case "a": this.touchA = value; break;
      case "b": this.touchB = value; break;
    }
  }
  
  /*XXX old idea, using touch events on <body>. i think we can do better
  _initTouch() {
    const element = document.body;
    element.addEventListener("touchstart", (event) => this.onTouchStart(event));
    element.addEventListener("touchend", (event) => this.onTouchEnd(event));
    element.addEventListener("touchcancel", (event) => this.onTouchCancel(event));
    element.addEventListener("touchmove", (event) => this.onTouchMove(event));
  }
  
  onTouchStart(event) {
    event.preventDefault();
    for (const touch of event.touches) {
      const myTouch = {
        identifier: touch.identifier,
        clientX: touch.clientX,
        clientY: touch.clientY,
        usage: this.chooseTouchUsage(touch.clientX, touch.clientY),
      };
      this.touches.push(myTouch);
      switch (myTouch.usage) {
        case "d": this.touchDpad = [0, 0]; break;
        case "a": this.touchA = true; break;
        case "b": this.touchB = true; break;
      }
    }
  }
  
  onTouchEnd(event) {
    event.preventDefault();
    for (const touch of event.changedTouches) {
      const p = this.touches.findIndex(t => t.identifier === touch.identifier);
      if (p < 0) continue;
      switch (this.touches[p].usage) {
        case "d": this.touchDpad = [0, 0]; break;
        case "a": this.touchA = false; break;
        case "b": this.touchB = false; break;
      }
      this.touches.splice(p, 1);
    }
  }
  
  onTouchCancel(event) {
    event.preventDefault();
    this.onTouchEnd(event);
  }
  
  onTouchMove(event) {
    event.preventDefault();
    for (const touch of event.changedTouches) {
      const myTouch = this.touches.find(t => t.identifier === touch.identifier);
      if (!myTouch) continue;
      switch (myTouch.usage) {
        case "d": {
            const dx = touch.clientX - myTouch.clientX;
            const dy = touch.clientY - myTouch.clientY;
            if (dx > this.touchDpadDistance) this.touchDpad[0] = 1;
            else if (dx < -this.touchDpadDistance) this.touchDpad[0] = -1;
            else this.touchDpad[0] = 0;
            if (dy > this.touchDpadDistance) this.touchDpad[1] = 1;
            else if (dy < -this.touchDpadDistance) this.touchDpad[1] = -1;
            else this.touchDpad[1] = 0;
          } break;
      }
    }
  }
  
  chooseTouchUsage(x, y) {
    const container = document.getElementById("game-container");
    if (!container) return "x";
    const bounds = container.getBoundingClientRect();
    if (y < bounds.top) return "x";
    if (y >= bounds.top + bounds.height) return "x";
    const midx = bounds.left + (bounds.width >> 1);
    if (x < midx) return "d";
    const midy = bounds.top + (bounds.height >> 1);
    if (y < midy) return "a";
    return "b";
  }
  /**/
}
