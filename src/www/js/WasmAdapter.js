/* WasmAdapter.js
 * Load, link, and communicate with the Wasm module.
 */
 
export class WasmAdapter {
  constructor() {
    this.instance = null;
    this.fbdirty = false;
    this.fb = null;
    this._input = 0;
    this.highscore = 0;
  }
  
  /* Download and instantiate.
   * Does not run any Wasm code.
   * Returns a Promise.
   */
  load(path) {
    const params = this._generateParams();
    return WebAssembly.instantiateStreaming(fetch(path), params).then((instance) => {
      this.instance = instance;
      return instance;
    });
  }
  
  /* Call setup() in Wasm code.
   */
  init() {
    this.instance.instance.exports.setup();
  }
  
  /* Call loop() in Wasm code, with the input state (see InputManager.js)
   */
  update(input) {
    this._input = input;
    this.instance.instance.exports.loop();
  }
  
  /* If we have received a framebuffer dirty notification from Wasm,
   * return it as a Uint8Array (96 * 64 * 2) and mark clean.
   * Otherwise return null.
   */
  getFramebufferIfDirty() {
    if (this.fbdirty) {
      this.fbdirty = false;
      return this.fb;
    }
    return null;
  }
  
  /* Private.
   ***********************************************************************/
   
  _generateParams() {
    return {
      env: {
        millis: () => Date.now(),
        micros: () => Date.now() * 1000,
        srand: () => {},
        rand: () => Math.floor(Math.random() * 2147483647),
        platform_init: (...args) => 1,
        platform_update: () => this._input,
        platform_send_framebuffer: (p) => this._receiveFramebuffer(p),
        abort: (...args) => {},
        usb_send: (...args) => {},
        tinysd_read: (dstp, size, pathp) => this._readHighScore(dstp, size, pathp),
        tinysd_write: (pathp, srcp, size) => this._writeHighScore(pathp, srcp, size),
      },
    };
  }
  
  _receiveFramebuffer(p) {
    if (typeof(p) !== "number") return;
    const buffer = this.instance.instance.exports.memory.buffer;
    const fblen = 96 * 64 * 2;
    if ((p < 0) || (p + fblen > buffer.byteLength)) return;
    this.fb = new Uint8Array(buffer, p, fblen);
    this.fbdirty = true;
  }
  
  // Strictly speaking it's "read file", but ivand only uses one file.
  _readHighScore(dstp, size, pathp) {
    if (size !== 4) return 0;
    const buffer = this.instance.instance.exports.memory.buffer;
    if ((typeof(dstp) !== "number") || (dstp < 0) || (dstp > buffer.byteLength - size)) return 0;
    const dst = new Uint8Array(buffer, dstp, 4);
    dst[0] = this.highscore;
    dst[1] = this.highscore >> 8;
    dst[2] = this.highscore >> 16;
    dst[3] = this.highscore >> 24;
    return 4;
  }
  
  _writeHighScore(pathp, srcp, size) {
    if (size !== 4) return 0;
    const buffer = this.instance.instance.exports.memory.buffer;
    if ((typeof(srcp) !== "number") || (srcp < 0) || (srcp > buffer.byteLength - size)) return 0;
    const src = new Uint8Array(buffer, srcp, 4);
    this.highscore = src[0] | (src[1] << 8) | (src[2] << 16) | (src[3] << 24);
    return 4;
  }
  
}
